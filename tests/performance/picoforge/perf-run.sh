#!/usr/bin/env bash
# picoforge mixed-load perf run on an INDEPENDENT mesh.
#
# Brings up a port-shifted, storage-isolated gateway stack (every 8xxx port →
# 9xxx, /tmp/picoforge → /tmp/picoforge-perf) so it never collides with a dev
# stack on the default ports. Then runs the `mixed` workload:
#
#   stage 1  pre-create up to SEED_USERS account records (population scale)
#   stage 2  CONNECTIONS workers, each a logged-in user issuing a weighted-
#            random stream of real actions (read / KV write / file commit /
#            issue / pipeline run / repo create / re-login) for DURATION secs
#
# The browser-facing webapp is also started (pointed at the perf gateway) so the
# UI can be tested visually IN PARALLEL with the load — see :$SIDE below. It is a
# sidecar, NOT a mesh service, so this script owns and stops it directly.
#
# Teardown: the mesh comes down through its control parent (SIGTERM → the
# parent's reaper takes the spawned backends down — never pkill / never kill a
# mesh child directly); the webapp sidecar (which the mesh did NOT spawn) is
# SIGTERM'd by this script. Both happen when the load finishes, so raise
# DURATION if you want a longer window to click around.
#
# Tunables (env or `make` vars): SEED_USERS CONNECTIONS DURATION REPOS_PER_WORKER
set -uo pipefail
cd "$(dirname "$0")/../../.."          # repo root

PICOMESH=./build-desktop-release/picomesh
PERF=./build-desktop-release/picoforge-perf
WEBAPP=./build-desktop-release/picoforge-webapp
SRC_CONFIG=assets/picoforge/config/picoforge.yaml
CONFIG=/tmp/picoforge-perf.yaml
DATADIR=/tmp/picoforge-perf
CTRL=9800
GW=9090
SIDE=9080   # browser-facing webapp (port-shifted 8080→9080)

# A SINGLE picoforge-perf process caps at ~9k req/s — not because the mesh is
# slow, but because one client process can only keep ~its-own-connections worth
# of requests in flight (throughput = concurrency / latency). The mesh itself
# does ~50k mixed. To drive it to saturation we run GENERATORS processes in
# parallel and sum their throughput. CONNECTIONS is PER generator.
GENERATORS=${GENERATORS:-8}
CONNECTIONS=${CONNECTIONS:-256}
DURATION=${DURATION:-60}
REPOS_PER_WORKER=${REPOS_PER_WORKER:-8}
# Connections self-register their own user (the throughput path). A non-zero
# pre-seed across many generators collides on usernames, so default it off.
SEED_USERS=${SEED_USERS:-0}

[ -x "$PICOMESH" ] && [ -x "$PERF" ] || {
    echo "build first: make build-desktop-release"; exit 1; }

# Port-shift + storage-isolate the canonical config (transform, don't fork it,
# so it can't drift from picoforge.yaml). 8xxx→9xxx is a uniform shift; the
# 9xxx space is otherwise unused, so there are no collisions.
sed -E 's/\b8([0-9]{3})\b/9\1/g; s#/tmp/picoforge#/tmp/picoforge-perf#g' \
    "$SRC_CONFIG" > "$CONFIG"

rm -rf "$DATADIR"; mkdir -p "$DATADIR" tmp

# The security refactor (gh#19) made the HS256 signing secret mandatory: the
# config has `security.jwt_secret: "${PICOMESH_JWT_SECRET}"` and the issuer also
# falls back to this env var. Without it token_issuer.login fails at "mint
# access" and EVERY login collapses (0 sessions established). The control parent
# must carry it so the spawned backends inherit it — mirror mesh-up.sh's default.
export PICOMESH_JWT_SECRET="${PICOMESH_JWT_SECRET:-picoforge-dev-mesh-secret-change-me}"

# Per-span trace shipping to trace_collector is pure overhead for a THROUGHPUT
# run (it alone caps read throughput ~3×). Default it OFF so each run measures
# real service cost; `PICOMESH_TELEMETRY=on make perf-picoforge` measures the
# tracing cost itself. The bottleneck report flags trace_collector if left on.
export PICOMESH_TELEMETRY="${PICOMESH_TELEMETRY:-off}"

PARENT=
WEBAPP_PID=
# The webapp is a SIDECAR this script owns (not a mesh service), so the script
# stops it directly — same as mesh-up.sh. The mesh parent (and only it) reaps
# the backends it spawned. Kill the webapp first (front tier), then the parent.
cleanup() {
    [ -n "$WEBAPP_PID" ] && kill -TERM "$WEBAPP_PID" 2>/dev/null || true
    [ -n "$PARENT" ] && kill -TERM "$PARENT" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "bringing up independent perf mesh (control :$CTRL, gateway :$GW, data $DATADIR)…"
"$PICOMESH" --config-file "$CONFIG" --frontend yhttp --host 127.0.0.1 --port "$CTRL" serve \
    > tmp/perf-mesh.log 2>&1 &
PARENT=$!

for i in $(seq 1 40); do ss -ltn 2>/dev/null | grep -qE ":$CTRL\b" && break; sleep 0.25; done
H=$(curl -sS --max-time 10 -XPOST -H 'Content-Type: application/json' \
     "http://127.0.0.1:$CTRL/create" -d '{"class":"mesh_mesh"}' \
     | sed -E 's/.*"handle":([0-9]+).*/\1/')
curl -sS --max-time 10 -XPOST -H 'Content-Type: application/json' \
     "http://127.0.0.1:$CTRL/invoke" \
     -d "{\"method\":\"mesh_mesh_reconcile_from_config\",\"handle\":$H,\"args\":[]}" >/dev/null

for i in $(seq 1 60); do ss -ltn 2>/dev/null | grep -qE ":$GW\b" && break; sleep 0.25; done
ss -ltn 2>/dev/null | grep -qE ":$GW\b" || { echo "gateway did not bind on :$GW (see tmp/perf-mesh.log)"; exit 1; }
sleep 2   # let every backend finish binding before the load starts

# Bring up the browser-facing webapp pointed at THIS perf gateway so the UI can
# be exercised visually IN PARALLEL with the load. The webapp uses SO_REUSEPORT;
# if :$SIDE is already held (a stale perf webapp), step to the next free port
# rather than silently share the listener.
if [ -x "$WEBAPP" ]; then
    while ss -ltn 2>/dev/null | grep -qE ":$SIDE\b" && [ "$SIDE" -lt "$GW" ]; do SIDE=$((SIDE + 1)); done
    "$WEBAPP" --gateway-url "http://127.0.0.1:$GW" \
        --host 127.0.0.1 --port "$SIDE" \
        --static assets/picoforge/static > tmp/perf-webapp.log 2>&1 &
    WEBAPP_PID=$!
    for i in $(seq 1 40); do ss -ltn 2>/dev/null | grep -qE ":$SIDE\b" && break; sleep 0.25; done
    echo "webapp up → http://127.0.0.1:$SIDE  (open it to test the UI while the load runs;"
    echo "            register/login there — it talks to the perf gateway on :$GW)"
else
    echo "note: $WEBAPP not built — skipping webapp (it ships with build-desktop-release)"
fi

# Per-node CPU snapshot from /proc (whole process, all threads incl the libuv
# pool) — no `perf` tool or privileges needed. comm is "picomesh" (no spaces),
# so /proc/<pid>/stat fields 14+15 (utime+stime jiffies) parse cleanly; ctxsw
# from /proc/<pid>/status. Only the mesh's own spawned nodes (children of the
# control parent) are sampled.
snap_cpu() {
    for pid in $(pgrep -P "$PARENT" 2>/dev/null); do
        svc=$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null | grep -oE -- '--name [^ ]+' | awk '{print $2}')
        jif=$(awk '{print $14+$15}' "/proc/$pid/stat" 2>/dev/null)
        csw=$(awk '/_ctxt_switches/{s+=$2} END{print s+0}' "/proc/$pid/status" 2>/dev/null)
        echo "$pid ${svc:-pid$pid} ${jif:-0} ${csw:-0}"
    done
}

curl -s "http://127.0.0.1:$GW/_perf?reset" >/dev/null 2>&1   # fresh op-latency window

echo "== picoforge mixed load: $GENERATORS generators × $CONNECTIONS connections each (= $((GENERATORS * CONNECTIONS)) total), duration=${DURATION}s =="
snap_cpu > tmp/perf-cpu-before.txt
rm -f tmp/perf-load-*.txt
T0=$SECONDS
for g in $(seq 1 "$GENERATORS"); do
    "$PERF" --host 127.0.0.1 --port "$GW" --scenario mixed \
        --seed-users "$SEED_USERS" --connections "$CONNECTIONS" \
        --duration "$DURATION" --repos-per-worker "$REPOS_PER_WORKER" \
        > "tmp/perf-load-$g.txt" 2>&1 &
done
wait
WINDOW=$((SECONDS - T0)); [ "$WINDOW" -lt 1 ] && WINDOW=1
snap_cpu > tmp/perf-cpu-after.txt
curl -s "http://127.0.0.1:$GW/_perf" > tmp/perf-ops.txt 2>&1

# Aggregate the N generator outputs into one summary perf-report.py understands
# (sum throughput / requests / sessions across all generators).
awk '
    /throughput/{thr+=$3}
    /requests/{ok+=$3; err+=$5}
    /established/{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){sess+=$i; tot+=$(i+2); break}}
    END{
        printf "scenario       : mixed (%d generators x %d connections)\n", G, C
        printf "concurrency    : %d total connections\n", G*C
        printf "sessions       : %d / %d established\n", sess, tot
        printf "requests       : %d ok, %d errors\n", ok, err
        printf "throughput     : %.1f req/s\n", thr
    }' G="$GENERATORS" C="$CONNECTIONS" tmp/perf-load-*.txt > tmp/perf-load.txt

cat tmp/perf-load.txt
python3 tests/performance/picoforge/perf-report.py \
    --load tmp/perf-load.txt --perf tmp/perf-ops.txt \
    --before tmp/perf-cpu-before.txt --after tmp/perf-cpu-after.txt \
    --window "$WINDOW" --ncpu "$(nproc 2>/dev/null || echo 0)"
