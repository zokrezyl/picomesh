#!/usr/bin/env python3
# Bottleneck report for a picoforge perf run. Reads:
#   --load   the picoforge-perf stdout (throughput / errors / latency / op mix)
#   --perf   the gateway GET /_perf op-latency aggregate (text table)
#   --before/--after  two /proc CPU snapshots ("pid svc utime+stime ctxsw")
#   --window seconds the load ran (to turn jiffy deltas into cores)
#   --ncpu   logical CPUs (for the idle headroom line)
# Prints a compact, ranked "where's the bottleneck + low-hanging fruit" summary.
import sys, re, argparse

HZ = 100  # CLK_TCK; deltas are in jiffies

def parse_snap(path):
    out = {}
    try:
        for ln in open(path):
            f = ln.split()
            if len(f) >= 4:
                out[f[0]] = (f[1], int(f[2]), int(f[3]))  # pid -> (svc, jiffies, ctxsw)
    except FileNotFoundError:
        pass
    return out

def grab(text, pat, cast=float, default=None):
    m = re.search(pat, text)
    return cast(m.group(1)) if m else default

ap = argparse.ArgumentParser()
for a in ("load", "perf", "before", "after"):
    ap.add_argument("--" + a, default="")
ap.add_argument("--window", type=float, default=1.0)
ap.add_argument("--ncpu", type=int, default=0)
args = ap.parse_args()

load = open(args.load).read() if args.load else ""
perf = open(args.perf).read() if args.perf else ""
win = max(args.window, 0.001)

thr = grab(load, r"throughput\s*:\s*([\d.]+)")
ok  = grab(load, r"requests\s*:\s*([\d]+)\s*ok", int, 0)
err = grab(load, r"ok,\s*([\d]+)\s*error", int, 0)
sess = re.search(r"sessions\s*:\s*(\d+)\s*/\s*(\d+)", load)
p50 = grab(load, r"p50=([\d.]+)"); p90 = grab(load, r"p90=([\d.]+)")
p99 = grab(load, r"p99=([\d.]+)"); pmax = grab(load, r"max=([\d.]+)")
errpct = 100.0 * err / max(ok + err, 1)

# per-node CPU from the two /proc snapshots
b, a = parse_snap(args.before), parse_snap(args.after)
nodes = []
for pid, (svc, ja, csa) in a.items():
    if pid in b:
        _, jb, csb = b[pid]
        cores = (ja - jb) / HZ / win
        csps = (csa - csb) / win
        if cores >= 0.02:
            nodes.append((svc, cores, csps))
nodes.sort(key=lambda x: -x[1])
busy = sum(c for _, c, _ in nodes)

# top ops from /_perf (op count p50 p90 p99 max, microseconds)
ops = []
for ln in perf.splitlines():
    f = ln.split()
    if len(f) == 6 and f[1].isdigit() and (f[0].startswith("rpc.") or f[0].startswith("gateway.")):
        ops.append((f[0], int(f[1]), int(f[3]), int(f[4]), int(f[5])))  # name,count,p90us,p99us,maxus

W = "═" * 64
print("\n" + W + "\n  BOTTLENECK REPORT" + "\n" + W)

corr = "✓ clean" if errpct < 1 else f"✗ {errpct:.1f}% ERRORS — correctness bug, fix before trusting throughput"
sline = f"   sessions {sess.group(1)}/{sess.group(2)}" if sess else ""
print(f"throughput : {thr or '?'} req/s    correctness: {corr}{sline}")
if p50 is not None:
    print(f"latency ms : p50 {p50}  p90 {p90}  p99 {p99}  max {pmax}")

if nodes:
    print("\ntop CPU nodes  (cores · ctxsw/s):")
    for svc, c, cs in nodes[:6]:
        tag = ""
        if cs / max(c, .01) > 15000: tag = "  ← high ctxsw/core = lock/fsync contention"
        print(f"   {svc:22}{c:5.2f}  {cs/1000:6.0f}k{tag}")
    if args.ncpu:
        idle = 100 * (1 - busy / args.ncpu)
        print(f"   busy {busy:.1f}/{args.ncpu} cores ({idle:.0f}% idle)")

if ops:
    print("\nslowest ops (p99 ms · count, from /_perf):")
    for name, cnt, p90u, p99u, mx in sorted(ops, key=lambda o: -o[3])[:6]:
        print(f"   {name:42}{p99u/1000:6.1f}  n={cnt}")

# ---- low-hanging fruit (data-driven) ----
fruit = []
if errpct >= 1:
    fruit.append(f"CORRECTNESS FIRST: {errpct:.1f}% errors — a fast-but-wrong run is meaningless.")
if any(s == "trace_collector" for s, _, _ in nodes):
    fruit.append("trace_collector is burning CPU → telemetry is ON. Run with PICOMESH_TELEMETRY=off.")
if args.ncpu and busy / args.ncpu < 0.6 and p50 and p90 and p90 > 3 * p50:
    fruit.append("box mostly idle but p90≫p50 → round-trip/contention bound, NOT CPU. "
                 "Cache token→uid (drops a session lookup per request); batch multi-commit writes.")
for s, c, cs in nodes:
    if cs / max(c, .01) > 15000 and "stor" in s:
        fruit.append(f"{s}: low work-per-ctxsw → fsync/lock contention. Group-commit or +shards.")
        break
hot = nodes[0][0] if nodes else None
if hot == "git_repo":
    fruit.append("git_repo hottest → libgit2 CPU (zlib+SHA1DC). Confirm zlib-ng linked; swap SHA1DC→SHA-NI.")
slow = max(ops, key=lambda o: o[3])[0] if ops else None
if slow and "issues" in slow:
    fruit.append("issues.open is slowest → it does 4 sequential storage commits; fold into 1 txn.")
print("\n▶ low-hanging fruit:")
for f in fruit or ["(nothing obvious — widen the load or check a longer window)"]:
    print(f"   - {f}")
print(W + "\n")
