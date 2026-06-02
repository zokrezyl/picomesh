#!/usr/bin/env bash
# picoforge smoke: launch picomesh with the scenario config, drive every
# plugin through one round-trip via the yttp JSON-RPC frontend.
#
# Expected to be run from the repo root after `make build-desktop-release`.

set -euo pipefail

PICOMESH=./build-desktop-release/picomesh
CONFIG=scenarios/picoforge/picoforge.yaml
PORT=8801

mkdir -p tmp

# Start the yttp server. The picoforge.yaml binds yttp on 0.0.0.0:8801 (see
# `mesh.services.yttp` in the scenario file).
"$PICOMESH" --config-file "$CONFIG" --frontend yttp --host 127.0.0.1 --port "$PORT" serve \
    > tmp/picoforge-srv.log 2>&1 &
SRV=$!
trap 'kill -INT $SRV 2>/dev/null; sleep 0.2; kill -KILL $SRV 2>/dev/null || true' EXIT
sleep 0.4

# rpc <method-json> — one request, frame it, print stripped response.
rpc() {
    local body=$1
    local len=${#body}
    printf 'Content-Length: %d\r\n\r\n%s' "$len" "$body" \
        | nc -q1 127.0.0.1 "$PORT" \
        | tr -d '\r' \
        | awk 'NR>2'
}

# create one instance of each class. We only need the class names that
# are wired into the linked binary; every plugin in the scenario maps
# to <plugin>_<class>.
declare -A H
for cls in portalloc_store session_store accounts_store \
           password_authn_store github_authn_store token_issuer_store \
           issues_store git_repo_store git_pipeline_store \
           personal_access_tokens_store mesh_store; do
    echo "== create $cls =="
    out=$(rpc '{"jsonrpc":"2.0","id":1,"method":"create","params":{"class":"'"$cls"'"}}')
    echo "  $out"
    H[$cls]=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
done

call() {
    local method=$1 args=$2 handle=$3
    rpc '{"jsonrpc":"2.0","id":2,"method":"invoke","params":{"method":"'"$method"'","handle":'"$handle"',"args":'"$args"'}}'
}

echo
echo "== portalloc.allocate(1) → port =="
call portalloc_store_allocate '[1]' "${H[portalloc_store]}"

echo
echo "== accounts.register(uid=100), set_balance(100, 5000), balance(100) =="
call accounts_store_register     '[100]'        "${H[accounts_store]}"
call accounts_store_set_balance  '[100, 5000]'  "${H[accounts_store]}"
call accounts_store_balance      '[100]'        "${H[accounts_store]}"

echo
echo "== password_authn.register(uid=100, hash=42), authenticate ok/bad =="
call password_authn_store_register      '[100, 42]'  "${H[password_authn_store]}"
call password_authn_store_authenticate  '[100, 42]'  "${H[password_authn_store]}"
call password_authn_store_authenticate  '[100, 43]'  "${H[password_authn_store]}"

echo
echo "== token_issuer.login(uid=100, provider=1) → tid, validate(tid) =="
out=$(call token_issuer_store_login '[100, 1]' "${H[token_issuer_store]}")
echo "  login: $out"
TID=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
call token_issuer_store_validate "[$TID]" "${H[token_issuer_store]}"

echo
echo "== personal_access_tokens.mint(uid=100) → pat, lookup =="
out=$(call personal_access_tokens_store_mint '[100]' "${H[personal_access_tokens_store]}")
echo "  mint: $out"
PAT=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
call personal_access_tokens_store_lookup "[$PAT]" "${H[personal_access_tokens_store]}"

echo
echo "== git_repo.make(owner=100) → repo, count_for_owner =="
out=$(call git_repo_store_make '[100]' "${H[git_repo_store]}")
echo "  make: $out"
REPO=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
call git_repo_store_count_for_owner '[100]' "${H[git_repo_store]}"

echo
echo "== issues.open(repo=$REPO, author=100), status, count_open_in_repo =="
out=$(call issues_store_open "[$REPO, 100]" "${H[issues_store]}")
ISS=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
echo "  open: $out"
call issues_store_status              "[$ISS]"   "${H[issues_store]}"
call issues_store_count_open_in_repo  "[$REPO]"  "${H[issues_store]}"

echo
echo "== git_pipeline.enqueue(repo=$REPO), lease(runner=1), count_running =="
out=$(call git_pipeline_store_enqueue "[$REPO]" "${H[git_pipeline_store]}")
JOB=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
echo "  enqueue: $out"
call git_pipeline_store_lease         '[1]'      "${H[git_pipeline_store]}"
call git_pipeline_store_count_running ''         "${H[git_pipeline_store]}"
call git_pipeline_store_complete      "[$JOB, 0]" "${H[git_pipeline_store]}"
call git_pipeline_store_count_done    ''         "${H[git_pipeline_store]}"

echo
echo "== mesh.register_service(service=10, port=8201), resolve(10), count =="
call mesh_store_register_service '[10, 8201]' "${H[mesh_store]}"
call mesh_store_resolve          '[10]'       "${H[mesh_store]}"
call mesh_store_count_services   ''           "${H[mesh_store]}"

echo
echo "== session.start(uid=100, provider=1) → sid, lookup =="
out=$(call session_store_start '[100, 1]' "${H[session_store]}")
SID=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
echo "  start: $out"
call session_store_lookup "[$SID]" "${H[session_store]}"

echo
echo "== github_authn.set_credentials(client=1, secret=2), register_code, resolve =="
call github_authn_store_set_credentials '[1, 2]'        "${H[github_authn_store]}"
call github_authn_store_register_code   '[12345, 100]'  "${H[github_authn_store]}"
call github_authn_store_resolve         '[12345]'       "${H[github_authn_store]}"

echo
echo "smoke: all backend plugins exercised end-to-end."
