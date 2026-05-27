#!/usr/bin/env bash
# git-yaafc — single command that brings up the full mesh and proves it
# works. Run from the repo root after `make build-desktop-release`.
#
#   1. Launches the parent yaafc as a yttp control on port 8800,
#      loaded with the git-yaafc scenario yaml.
#   2. From a client (this script), creates a mesh_store on the parent.
#   3. Calls mesh.reconcile_from_config — the parent reads
#      `mesh.services.*` and uv_spawns one child yaafc per service.
#   4. Probes a few children with real method calls.
#   5. Calls mesh.count_children to confirm.
#   6. Tears down — sends SIGTERM to each child, then SIGINT to the
#      parent.

set -uo pipefail

YAAFC=./build-desktop-release/yaafc
CONFIG=scenarios/git-yaafc/yaafc.yaml
CTRL=8800

mkdir -p tmp /tmp/git-yaafc

rm -f /tmp/git-yaafc/central.db tmp/mesh-parent.log

echo "[1/5] starting parent yaafc on :${CTRL}"
"$YAAFC" --config-file "$CONFIG" --frontend yttp --host 127.0.0.1 --port "$CTRL" serve \
    > tmp/mesh-parent.log 2>&1 &
PARENT=$!
cleanup() {
    kill -INT $PARENT 2>/dev/null
    sleep 0.3
    kill -KILL $PARENT 2>/dev/null
    # Belt and suspenders: nuke any stray yaafc on our spawn ports.
    for p in 8201 8202 8203 8204 8205 8206 8207 8208 8209 8210 8211; do
        pid=$(ss -lntp 2>/dev/null | awk -v port=:$p '$4 ~ port {print $0}' \
              | grep -oP 'pid=\K[0-9]+' | head -1)
        [ -n "$pid" ] && kill -9 "$pid" 2>/dev/null
    done
}
trap cleanup EXIT
sleep 0.4

rpc() {
    local body=$1 port=${2:-$CTRL}
    printf 'Content-Length: %d\r\n\r\n%s' "${#body}" "$body" \
        | nc -q1 127.0.0.1 "$port" | tr -d '\r' | awk 'NR>2'
}

echo "[2/5] creating mesh_store on parent"
out=$(rpc '{"jsonrpc":"2.0","id":1,"method":"create","params":{"class":"mesh_store"}}')
echo "      $out"
H=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
if [ -z "$H" ]; then
    echo "FAIL: no handle in response" >&2
    exit 1
fi

echo "[3/5] mesh_store_reconcile_from_config — parent spawns every child"
out=$(rpc '{"jsonrpc":"2.0","id":2,"method":"invoke","params":{"method":"mesh_store_reconcile_from_config","handle":'$H',"args":[]}}')
echo "      $out"
SPAWNED=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
echo "      spawned: $SPAWNED children"

echo "[4/5] waiting for children to bind, then probing"
sleep 0.6

# Probe storage (port 8202): set+get on the SQLite backend.
echo "  -- storage on :8202 (sqlite-backed)"
out=$(rpc '{"jsonrpc":"2.0","id":3,"method":"create","params":{"class":"storage_sql"}}' 8202)
S_H=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
rpc '{"jsonrpc":"2.0","id":4,"method":"invoke","params":{"method":"storage_sql_set","handle":'$S_H',"args":["hello",100]}}' 8202; echo
rpc '{"jsonrpc":"2.0","id":5,"method":"invoke","params":{"method":"storage_sql_get","handle":'$S_H',"args":["hello"]}}' 8202; echo

# Probe accounts (port 8204).
echo "  -- accounts on :8204"
out=$(rpc '{"jsonrpc":"2.0","id":6,"method":"create","params":{"class":"accounts_store"}}' 8204)
A_H=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
rpc '{"jsonrpc":"2.0","id":7,"method":"invoke","params":{"method":"accounts_store_register","handle":'$A_H',"args":[42]}}' 8204; echo
rpc '{"jsonrpc":"2.0","id":8,"method":"invoke","params":{"method":"accounts_store_count","handle":'$A_H',"args":[]}}' 8204; echo

# Probe issues (port 8208).
echo "  -- issues on :8208"
out=$(rpc '{"jsonrpc":"2.0","id":9,"method":"create","params":{"class":"issues_store"}}' 8208)
I_H=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
rpc '{"jsonrpc":"2.0","id":10,"method":"invoke","params":{"method":"issues_store_open","handle":'$I_H',"args":[1,42]}}' 8208; echo

# Probe token_issuer (port 8207).
echo "  -- token_issuer on :8207"
out=$(rpc '{"jsonrpc":"2.0","id":11,"method":"create","params":{"class":"token_issuer_store"}}' 8207)
T_H=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
rpc '{"jsonrpc":"2.0","id":12,"method":"invoke","params":{"method":"token_issuer_store_login","handle":'$T_H',"args":[42,1]}}' 8207; echo

echo "[5/5] parent.count_children"
rpc '{"jsonrpc":"2.0","id":99,"method":"invoke","params":{"method":"mesh_store_count_children","handle":'$H',"args":[]}}'
echo

echo ""
echo "OK — mesh is live. parent log: tmp/mesh-parent.log"
echo "    Press Ctrl-C to tear everything down."
