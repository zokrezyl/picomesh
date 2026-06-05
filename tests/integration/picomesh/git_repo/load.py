#!/usr/bin/env python3
# Max-throughput driver for an isolated git_repo node. Spawns N worker PROCESSES
# (real parallelism), each committing to its OWN bare repo (parallel across
# repos, like real usage). put_file is owner-gated, so we mint a site:owner JWT
# (HS256 with the node's test secret) and present it as a Bearer — the node's
# bearer_jwt authenticator verifies it and git_repo sees a valid caller.
# Reports commits/s (the libgit2 zlib+SHA commit ceiling) + a read rate.
import http.client, sys, time, json, hmac, hashlib, base64
from multiprocessing import Process, Queue

HOST = "127.0.0.1"
PORT = int(sys.argv[1])
NPROC = int(sys.argv[2]) if len(sys.argv) > 2 else 8
DUR = float(sys.argv[3]) if len(sys.argv) > 3 else 15.0
SECRET = (sys.argv[4] if len(sys.argv) > 4 else "picomesh-it-git-secret").encode()

def b64url(x):
    return base64.urlsafe_b64encode(x).rstrip(b"=")

def mint(uid, groups):
    hdr = b64url(b'{"alg":"HS256","typ":"JWT"}')
    now = int(time.time())
    payload = b64url(json.dumps(
        {"iss": "picomesh", "sub": uid, "username": f"u{uid}", "groups": groups,
         "iat": now, "exp": now + 3600}, separators=(",", ":")).encode())
    sig = b64url(hmac.new(SECRET, hdr + b"." + payload, hashlib.sha256).digest())
    return (hdr + b"." + payload + b"." + sig).decode()

JWT = mint(1000, ["site:owner"])   # site-admin can write any repo

def worker(wid, q):
    c = http.client.HTTPConnection(HOST, PORT)
    hdr = {"Content-Type": "application/json", "Authorization": "Bearer " + JWT}
    def rpc(path, args):
        c.request("POST", "/_rpc", json.dumps({"path": path, "args": args}), hdr)
        r = c.getresponse(); data = r.read()
        return (r.status == 200 and b'"error"' not in data), data
    # each worker commits to its OWN bare repo (parallel across repos)
    ok, data = rpc("git_repo.git_repo.make", [1000, "loadtest", f"r{wid}"])
    rid = json.loads(data).get("result", 0) if ok else 0

    commits = reads = cerr = rerr = 0
    t_commit = t_read = 0.0
    n = 0
    end = time.time() + DUR
    while time.time() < end:
        n += 1
        # commit: overwrite a fixed file so the tree stays small → measures the
        # pure commit cost (blob+tree+commit deflate/SHA), not tree growth.
        t0 = time.time()
        ok, _ = rpc("git_repo.git_repo.put_file",
                    [rid, "file.txt", f"content {n}", f"commit {n}", "", ""])
        t_commit += time.time() - t0; commits += 1; cerr += 0 if ok else 1
        # read: repo count (storage read)
        t0 = time.time()
        ok, _ = rpc("git_repo.git_repo.count_total", [])
        t_read += time.time() - t0; reads += 1; rerr += 0 if ok else 1
    q.put((commits, reads, cerr, rerr, t_commit, t_read))

def main():
    q = Queue()
    procs = [Process(target=worker, args=(i, q)) for i in range(NPROC)]
    t0 = time.time()
    for p in procs: p.start()
    for p in procs: p.join()
    dt = time.time() - t0
    C = R = CE = RE = TC = TR = 0
    for _ in procs:
        c, r, ce, re_, tc, tr = q.get()
        C += c; R += r; CE += ce; RE += re_; TC += tc; TR += tr
    print(f"git_repo  conns={NPROC}  wall={dt:.1f}s")
    print(f"  THROUGHPUT : {(C+R)/dt:,.0f} ops/s   ({C/dt:,.0f} commits/s + {R/dt:,.0f} reads/s)")
    print(f"  errors     : commit {CE} ({100*CE/max(C,1):.1f}%)   read {RE} ({100*RE/max(R,1):.1f}%)")
    print(f"  mean lat   : commit {1000*TC/max(C,1):.2f} ms   read {1000*TR/max(R,1):.2f} ms")

if __name__ == "__main__":
    main()
