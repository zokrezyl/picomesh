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
# The mesh is torn down through its control parent (SIGTERM → the parent's
# reaper takes the backends down — never pkill / never kill a child directly).
#
# Tunables (env or `make` vars): SEED_USERS CONNECTIONS DURATION REPOS_PER_WORKER
set -uo pipefail
cd "$(dirname "$0")/../../.."          # repo root

PICOMESH=./build-desktop-release/picomesh
PERF=./build-desktop-release/picoforge-perf
SRC_CONFIG=scenarios/picoforge/picoforge.yaml
CONFIG=/tmp/picoforge-perf.yaml
DATADIR=/tmp/picoforge-perf
CTRL=9800
GW=9080

SEED_USERS=${SEED_USERS:-50000}
CONNECTIONS=${CONNECTIONS:-32}
DURATION=${DURATION:-60}
REPOS_PER_WORKER=${REPOS_PER_WORKER:-8}

[ -x "$PICOMESH" ] && [ -x "$PERF" ] || {
    echo "build first: make build-desktop-release"; exit 1; }

# Port-shift + storage-isolate the canonical config (transform, don't fork it,
# so it can't drift from picoforge.yaml). 8xxx→9xxx is a uniform shift; the
# 9xxx space is otherwise unused, so there are no collisions.
sed -E 's/\b8([0-9]{3})\b/9\1/g; s#/tmp/picoforge#/tmp/picoforge-perf#g' \
    "$SRC_CONFIG" > "$CONFIG"

rm -rf "$DATADIR"; mkdir -p "$DATADIR" tmp

PARENT=
cleanup() { [ -n "$PARENT" ] && kill -TERM "$PARENT" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "bringing up independent perf mesh (control :$CTRL, gateway :$GW, data $DATADIR)…"
"$PICOMESH" --config-file "$CONFIG" --frontend yhttp --host 127.0.0.1 --port "$CTRL" serve \
    > tmp/perf-mesh.log 2>&1 &
PARENT=$!

for i in $(seq 1 40); do ss -ltn 2>/dev/null | grep -qE ":$CTRL\b" && break; sleep 0.25; done
H=$(curl -sS --max-time 10 -XPOST -H 'Content-Type: application/json' \
     "http://127.0.0.1:$CTRL/create" -d '{"class":"mesh_store"}' \
     | sed -E 's/.*"handle":([0-9]+).*/\1/')
curl -sS --max-time 10 -XPOST -H 'Content-Type: application/json' \
     "http://127.0.0.1:$CTRL/invoke" \
     -d "{\"method\":\"mesh_store_reconcile_from_config\",\"handle\":$H,\"args\":[]}" >/dev/null

for i in $(seq 1 60); do ss -ltn 2>/dev/null | grep -qE ":$GW\b" && break; sleep 0.25; done
ss -ltn 2>/dev/null | grep -qE ":$GW\b" || { echo "gateway did not bind on :$GW (see tmp/perf-mesh.log)"; exit 1; }
sleep 2   # let every backend finish binding before the load starts

echo "== picoforge mixed load: seed=$SEED_USERS connections=$CONNECTIONS duration=${DURATION}s =="
"$PERF" --host 127.0.0.1 --port "$GW" --scenario mixed \
    --seed-users "$SEED_USERS" --connections "$CONNECTIONS" \
    --duration "$DURATION" --repos-per-worker "$REPOS_PER_WORKER"
