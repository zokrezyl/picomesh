#!/usr/bin/env bash
# Isolated max-throughput test for the relational_storage plugin.
#
# Runs ONE picomesh process with ONLY [relational_storage] (no gateway, no auth,
# no other services) loaded with picomesh's MAIN product schema, then hammers
# db.exec / db.query directly through the control plane (/create + /invoke) and
# reports ops/s. This isolates the relational store's own throughput ceiling —
# if the mesh's bottleneck is here, this exposes it without the gateway/session
# layers in the way.
#
# Knobs (env): PORT WORKERS SHARDS CONNS DURATION
set -uo pipefail
cd "$(dirname "$0")/../../../.."   # repo root

PICOMESH=./build-desktop-release/picomesh
CFG=/tmp/picomesh-it-rel.yaml
DATA=/tmp/picomesh-it-rel
PORT=${PORT:-7710}
WORKERS=${WORKERS:-4}        # matches the rstore_* tier in picoforge.yaml
SHARDS=${SHARDS:-8}
CONNS=${CONNS:-24}            # concurrent client PROCESSES
DURATION=${DURATION:-15}

[ -x "$PICOMESH" ] || { echo "build first: make build-desktop-release"; exit 1; }
rm -rf "$DATA"; mkdir -p "$DATA" tmp

# Standalone single-plugin config. No `remotes:` ⇒ the node is neither gateway
# nor bridge ⇒ control-plane /create+/invoke (unauthenticated, exactly what a
# direct throughput probe wants). The schema below MIRRORS the rstore_uid
# product schema in assets/picoforge/config/picoforge.yaml — keep in sync.
cat > "$CFG" <<EOF
plugins: ["relational_storage"]
workers: $WORKERS
relational_storage:
  shards: $SHARDS
  path: $DATA
  schema: |
    CREATE TABLE IF NOT EXISTS namespaces(
      id INTEGER PRIMARY KEY,
      kind TEXT NOT NULL DEFAULT 'user',
      name TEXT NOT NULL,
      owner_uid INTEGER NOT NULL,
      created_at INTEGER NOT NULL DEFAULT 0,
      UNIQUE(kind, name));
    CREATE TABLE IF NOT EXISTS repos(
      id INTEGER PRIMARY KEY,
      namespace_id INTEGER NOT NULL,
      name TEXT NOT NULL,
      owner_uid INTEGER NOT NULL,
      visibility TEXT NOT NULL DEFAULT 'private',
      created_at INTEGER NOT NULL DEFAULT 0,
      UNIQUE(namespace_id, name));
    CREATE TABLE IF NOT EXISTS repo_members(
      repo_id INTEGER NOT NULL,
      uid INTEGER NOT NULL,
      role TEXT NOT NULL DEFAULT 'member',
      PRIMARY KEY(repo_id, uid));
    CREATE TABLE IF NOT EXISTS issues(
      id INTEGER PRIMARY KEY,
      repo_id INTEGER NOT NULL,
      number INTEGER NOT NULL,
      title TEXT,
      status TEXT NOT NULL DEFAULT 'open',
      author_uid INTEGER NOT NULL,
      created_at INTEGER NOT NULL DEFAULT 0,
      UNIQUE(repo_id, number));
    CREATE TABLE IF NOT EXISTS pipeline_runs(
      id INTEGER PRIMARY KEY,
      repo_id INTEGER NOT NULL,
      status TEXT NOT NULL DEFAULT 'pending',
      created_by_uid INTEGER NOT NULL,
      created_at INTEGER NOT NULL DEFAULT 0);
    CREATE INDEX IF NOT EXISTS ix_repos_owner ON repos(owner_uid);
    CREATE INDEX IF NOT EXISTS ix_issues_repo ON issues(repo_id);
    CREATE INDEX IF NOT EXISTS ix_runs_repo   ON pipeline_runs(repo_id);
    CREATE INDEX IF NOT EXISTS ix_members_uid ON repo_members(uid);
# A (dummy) remote puts the node in BRIDGE mode so it serves the STATELESS
# path-based /_rpc (object create+invoke+release per call — the same shape the
# mesh uses internally) instead of the per-worker /invoke handle model, which
# breaks across SO_REUSEPORT workers. trace_collector isn't running; with
# telemetry off the span ship-out is a no-op, so the remote is never dialed.
remotes:
  - service: trace_collector
    port: 9999
EOF

NODE=
cleanup() { [ -n "$NODE" ] && kill -TERM "$NODE" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

export PICOMESH_TELEMETRY=off   # no span ship-out; keep the dummy remote idle
echo "== relational_storage isolated: workers=$WORKERS shards=$SHARDS conns=$CONNS duration=${DURATION}s =="
"$PICOMESH" --config-file "$CFG" --frontend yhttp --host 127.0.0.1 --port "$PORT" serve > tmp/it-rel.log 2>&1 &
NODE=$!
for i in $(seq 1 40); do ss -ltn 2>/dev/null | grep -qE ":$PORT\b" && break; sleep 0.25; done
ss -ltn 2>/dev/null | grep -qE ":$PORT\b" || { echo "node did not bind on :$PORT (see tmp/it-rel.log)"; tail -5 tmp/it-rel.log; exit 1; }

python3 tests/integration/picomesh/relational_storage/load.py "$PORT" "$CONNS" "$DURATION"
