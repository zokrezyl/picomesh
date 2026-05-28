#!/usr/bin/env bash
# git-yaafc — bring up the full mesh and prove it works end-to-end.
#
# Layout:
#
#   parent yaafc --frontend yhttp --port 8800        ← control channel (REST)
#     ↳ mesh_store_reconcile_from_config spawns one child per service
#       in mesh.services.* on the service's own port. Each backend
#       child uses --frontend yrpc (binary, for inter-service RPC).
#       The 'frontend' service (port 8080) overrides this with
#       `frontend: yhttp` in its YAML block — it serves the HTML UI
#       that browsers / curl can hit, and it opens yrpc client
#       sessions to every backend via its `remotes:` block.
#
# Each child inherits --config-file AND --name <self> from the parent.
# The engine's service-projection step (gh#1) merges
# `mesh.services.<self>.config` onto the child's config root so plugins
# find their config at natural paths.
#
# Run from the repo root after `make build-desktop-release`.

set -uo pipefail

YAAFC=./build-desktop-release/yaafc
CONFIG=scenarios/git-yaafc/yaafc.yaml
CTRL=8800
WEB=8080
DB=/tmp/git-yaafc/central.db

mkdir -p tmp /tmp/git-yaafc
rm -f "$DB" tmp/mesh-parent.log

PASS=0
FAIL=0
note_pass() { PASS=$((PASS+1)); echo "  PASS: $1"; }
note_fail() { FAIL=$((FAIL+1)); echo "  FAIL: $1" >&2; }

echo "[1/6] starting parent yaafc (yhttp control) on :${CTRL}"
"$YAAFC" --config-file "$CONFIG" --frontend yhttp --host 127.0.0.1 --port "$CTRL" serve \
    > tmp/mesh-parent.log 2>&1 &
PARENT=$!
cleanup() {
    pkill -9 -f 'yaafc.*serve' 2>/dev/null || true
    kill -KILL $PARENT 2>/dev/null || true
}
trap cleanup EXIT INT TERM
sleep 0.5

http_post() {
    local port=$1 path=$2 body=$3
    curl -sS --max-time 10 -XPOST -H 'Content-Type: application/json' \
         "http://127.0.0.1:$port$path" -d "$body"
}
http_form() {
    local port=$1 path=$2; shift 2
    curl -sS --max-time 10 -b tmp/cookies.txt -c tmp/cookies.txt -XPOST \
         "http://127.0.0.1:$port$path" "$@"
}
http_get() {
    local port=$1 path=$2
    curl -sS --max-time 10 -b tmp/cookies.txt -c tmp/cookies.txt \
         "http://127.0.0.1:$port$path"
}
expect_contains() {
    local label=$1 resp=$2 needle=$3
    if [[ "$resp" =~ $needle ]]; then
        note_pass "$label"
    else
        note_fail "$label — got: ${resp:0:200}"
    fi
}

echo "[2/6] create mesh_store on parent"
out=$(http_post "$CTRL" /create '{"class":"mesh_store"}')
H=$(echo "$out" | sed -E 's/.*"handle":([0-9]+).*/\1/')
expect_contains 'mesh_store create' "$out" '"handle":[0-9]+'

echo "[3/6] mesh.reconcile_from_config — spawn every service as a child"
out=$(http_post "$CTRL" /invoke "{\"method\":\"mesh_store_reconcile_from_config\",\"handle\":$H,\"args\":[]}")
expect_contains 'reconcile spawn count > 0' "$out" '"result":[1-9][0-9]*'
SPAWNED=$(echo "$out" | sed -E 's/.*"result":([0-9]+).*/\1/')
echo "  spawned: $SPAWNED children"

echo "[4/6] waiting for children to bind…"
sleep 1.5

echo "[5/6] exercising the HTML frontend on :${WEB}"
rm -f tmp/cookies.txt
touch tmp/cookies.txt

# Landing page.
out=$(http_get "$WEB" /login)
expect_contains 'GET /login renders sign-in card' "$out" '<h1>Sign in</h1>'
expect_contains 'GET /login shows username field'  "$out" 'name="username"'
expect_contains 'GET /login shows password field'  "$out" 'name="password"'
expect_contains 'GET /login pulls htmx (like yaapp)' "$out" 'unpkg.com/htmx.org'
if echo "$out" | grep -q '<li><a href="/repos">Repos</a></li>'; then
    note_fail "service nav links visible to anonymous"
else
    note_pass "anonymous sees no service nav"
fi

# Anonymous visitor on a protected page → redirect to /login.
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null http://127.0.0.1:$WEB/repos)
expect_contains 'GET /repos as anon → 303 /login' "$hdrs" '303 See Other.*[Ll]ocation: /login|[Ll]ocation: /login.*303'

# yaapp split: /login does NOT auto-register. Cold sign-in must fail
# with "no such user" before we visit /register.
out=$(curl -sS --max-time 10 -XPOST http://127.0.0.1:$WEB/login \
           --data-urlencode 'username=alice' --data-urlencode 'password=hunter2')
expect_contains 'POST /login on unregistered user fails' "$out" 'no such user'

# Register → creates account, mints session, redirects to /repos.
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -c tmp/cookies.txt \
            -XPOST http://127.0.0.1:$WEB/register \
            --data-urlencode 'username=alice' --data-urlencode 'password=hunter2')
expect_contains 'POST /register → 303 /repos'   "$hdrs" '303 See Other'
expect_contains 'POST /register sets sid cookie' "$hdrs" 'Set-Cookie: yaafc-sid='
expect_contains 'POST /register sets uname cookie' "$hdrs" 'Set-Cookie: yaafc-uname=alice'

# A second /register for the same name must be rejected.
out=$(curl -sS --max-time 10 -XPOST http://127.0.0.1:$WEB/register \
           --data-urlencode 'username=alice' --data-urlencode 'password=hunter2')
expect_contains 'POST /register rejects duplicate' "$out" 'already taken'

# Sign-out invalidates the session server-side AND wipes cookies.
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -b tmp/cookies.txt -c tmp/cookies.txt \
            -XPOST http://127.0.0.1:$WEB/logout)
expect_contains 'POST /logout → 303 /login'        "$hdrs" '303 See Other'
expect_contains 'POST /logout clears sid cookie'   "$hdrs" 'yaafc-sid=;'
expect_contains 'POST /logout clears uname cookie' "$hdrs" 'yaafc-uname=;'

# Re-login (now that the account exists from /register).
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -c tmp/cookies.txt \
            -XPOST http://127.0.0.1:$WEB/login \
            --data-urlencode 'username=alice' --data-urlencode 'password=hunter2')
expect_contains 'POST /login → 303 /repos'  "$hdrs" '303 See Other'
expect_contains 'POST /login sets sid cookie' "$hdrs" 'Set-Cookie: yaafc-sid='

# Create a repo under /alice/website via /repos/new (POST `name`).
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -b tmp/cookies.txt \
            -XPOST http://127.0.0.1:$WEB/repos/new \
            --data-urlencode 'name=website')
expect_contains 'POST /repos/new → 303 /alice/website' "$hdrs" '303 See Other.*/alice/website|/alice/website.*303'

# Authenticated pages with the new URL shape.
out=$(http_get "$WEB" /repos)
expect_contains 'GET /repos renders'                "$out" '<h1>Repositories</h1>'
out=$(http_get "$WEB" /alice)
expect_contains 'GET /alice (account landing)'      "$out" '<h1>@alice</h1>'
out=$(http_get "$WEB" /alice/website)
expect_contains 'GET /alice/website (repo show)'    "$out" 'alice</a>/website'
out=$(http_get "$WEB" /alice/website/issues)
expect_contains 'GET /alice/website/issues renders' "$out" '<h1>Issues</h1>'
out=$(http_get "$WEB" /alice/website/runs)
expect_contains 'GET /alice/website/runs renders'   "$out" 'Pipeline'
out=$(http_get "$WEB" /admin/users)
expect_contains 'GET /admin/users renders'          "$out" 'Users'
out=$(http_get "$WEB" /admin/storage)
expect_contains 'GET /admin/storage renders'        "$out" 'Storage'

# Storage round-trip through the admin form.
out=$(curl -sS --max-time 10 -b tmp/cookies.txt \
           -XPOST http://127.0.0.1:$WEB/admin/storage/set \
           --data-urlencode 'key=hello' --data-urlencode 'value=42')
expect_contains 'POST /admin/storage/set echoes recent set' "$out" 'last set: <code>hello</code> = <code>42</code>'
out=$(http_get "$WEB" /admin/storage)
expect_contains 'GET /admin/storage reports row count' "$out" '>[1-9][0-9]* rows<'

# Open an issue through the HTML form.
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -b tmp/cookies.txt \
            -XPOST http://127.0.0.1:$WEB/alice/website/issues/new)
expect_contains 'POST /alice/website/issues/new → 303' "$hdrs" '303 See Other'

# Parent control still alive.
out=$(http_post "$CTRL" /invoke "{\"method\":\"mesh_store_count_children\",\"handle\":$H,\"args\":[]}")
expect_contains "parent.count_children == $SPAWNED" "$out" "\"result\":$SPAWNED"

echo
echo "[6/6] sqlite persistence proof — storage child must have written $DB"
if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "  SKIP: sqlite3 cli not installed — cannot verify on-disk bytes"
elif [ ! -s "$DB" ]; then
    note_fail "$DB does not exist or is empty (child still used :memory:?)"
else
    rows=$(sqlite3 "$DB" "SELECT k || '=' || v FROM kv;" 2>/dev/null) || rows=''
    if [ -z "$rows" ]; then
        if sqlite3 "$DB" 'SELECT name FROM sqlite_master;' 2>/dev/null | grep -q '^kv$'; then
            note_fail "kv table present but no rows"
        else
            note_fail "kv table missing — child likely opened :memory: instead of $DB"
        fi
    elif echo "$rows" | grep -q '^hello=42$'; then
        note_pass "sqlite db on disk contains hello=42"
    else
        note_fail "sqlite db contents unexpected: $rows"
    fi
fi
ls -la "$DB" 2>/dev/null | head -1

echo
echo "========================================"
echo "PASS: $PASS    FAIL: $FAIL"
echo "========================================"
if [ "$FAIL" -gt 0 ]; then
    echo "parent log: tmp/mesh-parent.log"
    exit 1
fi
echo ""
echo "OK — mesh is live. parent log: tmp/mesh-parent.log"
echo "    Browser:"
echo "      open http://127.0.0.1:${WEB}/login   (server-side HTML)"
echo "    Control plane:"
echo "      curl http://127.0.0.1:${CTRL}/        (mesh REST)"
echo "    Press Ctrl-C to tear everything down."

# Honour the message above: stay alive until the user kills us, or until
# the parent yaafc dies on its own. Without this the script exits and
# the EXIT trap SIGKILLs the parent — the mesh would look "crashed" to
# anyone trying to open the URL.
#
# `wait $PID` is documented to be interruptible by traps, but in practice
# bash only fires the trap after the awaited PID exits — so SIGINT from
# the terminal stays queued and the user thinks Ctrl-C is dead. A poll
# loop with a short sleep gives the trap a chance to fire promptly.
while kill -0 "$PARENT" 2>/dev/null; do
    sleep 1
done
