#!/usr/bin/env bash
# Isolated max-throughput test for the git_repo plugin (+ collocated
# sharded_storage for repo metadata).
#
# Runs ONE picomesh process with ONLY [git_repo, sharded_storage] in gateway
# mode with a bearer-JWT authenticator + permissive authorizer. put_file is
# owner-gated on a VERIFIED JWT, so the driver mints a site:owner token and
# presents it as a Bearer — this exercises the real owner-checked libgit2 commit
# path while keeping the node to just these two plugins (no session/accounts
# tier). Reports commits/s — git_repo's libgit2 (zlib + SHA) commit ceiling.
#
# Knobs (env): PORT WORKERS SHARDS CONNS DURATION
set -uo pipefail
cd "$(dirname "$0")/../../../.."   # repo root

PICOMESH=./build-desktop-release/picomesh
CFG=/tmp/picomesh-it-git.yaml
DATA=/tmp/picomesh-it-git
PORT=${PORT:-7720}
WORKERS=${WORKERS:-4}       # matches git_repo's tier in picoforge.yaml
SHARDS=${SHARDS:-8}
CONNS=${CONNS:-24}           # concurrent client PROCESSES (each its own bare repo)
DURATION=${DURATION:-15}
SECRET=picomesh-it-git-secret

[ -x "$PICOMESH" ] || { echo "build first: make build-desktop-release"; exit 1; }
rm -rf "$DATA"; mkdir -p "$DATA/repos" "$DATA/kv" tmp

cat > "$CFG" <<EOF
plugins: ["git_repo", "sharded_storage"]
workers: $WORKERS
git_repo:
  repos_dir: $DATA/repos
sharded_storage:
  shards: $SHARDS
  path: $DATA/kv
# Gateway mode (serve_app) so the path-based /_rpc runs the authn chain: a
# bearer-JWT authenticator verifies the driver's minted token and forwards it as
# the backend's "jwt" header, satisfying put_file's owner check. The authorizer
# is permissive ('none') — this is a throughput probe, not an authz test.
yhttp:
  serve_app: true
security:
  jwt_secret: "$SECRET"
  authenticators:
    - type: bearer_jwt_token
  authorizer:
    type: none
EOF

NODE=
cleanup() { [ -n "$NODE" ] && kill -TERM "$NODE" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

export PICOMESH_TELEMETRY=off
echo "== git_repo isolated: workers=$WORKERS shards=$SHARDS conns=$CONNS duration=${DURATION}s =="
"$PICOMESH" --config-file "$CFG" --frontend yhttp --host 127.0.0.1 --port "$PORT" serve > tmp/it-git.log 2>&1 &
NODE=$!
for i in $(seq 1 40); do ss -ltn 2>/dev/null | grep -qE ":$PORT\b" && break; sleep 0.25; done
ss -ltn 2>/dev/null | grep -qE ":$PORT\b" || { echo "node did not bind on :$PORT (see tmp/it-git.log)"; tail -5 tmp/it-git.log; exit 1; }

python3 tests/integration/picomesh/git_repo/load.py "$PORT" "$CONNS" "$DURATION" "$SECRET"
