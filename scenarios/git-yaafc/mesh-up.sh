#!/usr/bin/env bash
# git-yaafc — bring up the full mesh and prove it works end-to-end.
#
# Layout:
#
#   parent yaafc --frontend yhttp --port 8800        ← control channel (REST)
#     ↳ mesh_store_reconcile_from_config spawns one child per service
#       in mesh.services.* on the service's own port. Each backend
#       child uses --frontend yrpc (binary, for inter-service RPC).
#       The 'gateway' service (port 8080) overrides this with
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

# Register → creates account, mints session, redirects to /alice
# (GitHub-style namespace landing, mirroring yaapp's _landing_url).
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -c tmp/cookies.txt \
            -XPOST http://127.0.0.1:$WEB/register \
            --data-urlencode 'username=alice' --data-urlencode 'password=hunter2')
expect_contains 'POST /register → 303 /alice'   "$hdrs" '303 See Other.*[Ll]ocation: /alice|[Ll]ocation: /alice.*303'
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
expect_contains 'POST /login → 303 /alice'   "$hdrs" '303 See Other.*[Ll]ocation: /alice|[Ll]ocation: /alice.*303'
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
# /admin/users is gated on the site-owner role. alice is the very
# first user to /register, so she's auto-promoted at register time.
out=$(http_get "$WEB" /admin/users)
expect_contains 'GET /admin/users renders for site owner' "$out" '<h1>Users</h1>'

# /admin/storage is removed entirely — backend kv state isn't a UI
# surface anymore. Even the site owner should not get an HTML page
# there. A direct hit should not render the old form.
out=$(http_get "$WEB" /admin/storage)
if echo "$out" | grep -q 'storage_sql\|kv table'; then
    note_fail "/admin/storage still exposes backend kv internals"
else
    note_pass "/admin/storage no longer exposes storage internals"
fi

# A non-owner account must NOT reach /admin/users. Register a second
# user (gets role=0), and verify they get bounced to /repos.
curl -sS --max-time 10 -c tmp/bob-cookies.txt \
     -XPOST http://127.0.0.1:$WEB/register \
     --data-urlencode 'username=bob' --data-urlencode 'password=bobsecret' \
     -o /dev/null
hdrs=$(curl -sS --max-time 10 -D - -o /dev/null -b tmp/bob-cookies.txt \
            http://127.0.0.1:$WEB/admin/users)
expect_contains 'non-owner /admin/users → 303 /bob' "$hdrs" '303 See Other.*[Ll]ocation: /bob|[Ll]ocation: /bob.*303'
# And the nav for bob must not advertise /admin/users at all.
out=$(curl -sS --max-time 10 -b tmp/bob-cookies.txt http://127.0.0.1:$WEB/repos)
if echo "$out" | grep -q '<a href="/admin/users">Users</a>'; then
    note_fail "non-owner sees /admin/users link in nav"
else
    note_pass "non-owner does NOT see admin links in nav"
fi

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
    # The storage plugin maps each logical context to a table named
    # kv_<context>. The register/login flow exercises three contexts:
    # accounts, session, password_authn.
    accounts_rows=$(sqlite3 "$DB" "SELECT k || '=' || v FROM kv_accounts;" 2>/dev/null) || accounts_rows=''
    session_rows=$(sqlite3 "$DB" "SELECT k || '=' || v FROM kv_session;" 2>/dev/null) || session_rows=''
    pw_rows=$(sqlite3 "$DB" "SELECT k || '=' || v FROM kv_password_authn;" 2>/dev/null) || pw_rows=''

    if [ -z "$accounts_rows" ] && [ -z "$session_rows" ] && [ -z "$pw_rows" ]; then
        tables=$(sqlite3 "$DB" 'SELECT name FROM sqlite_master;' 2>/dev/null)
        note_fail "kv_<context> tables missing or empty — child likely opened :memory: instead of $DB (tables present: $tables)"
    else
        if echo "$accounts_rows" | grep -q '^count=' && \
           echo "$pw_rows" | grep -q '^count=' && \
           echo "$session_rows" | grep -q '^next_sid='; then
            note_pass "sqlite db on disk has account/session state"
        else
            note_fail "expected per-context keys missing — accounts: $accounts_rows ; session: $session_rows ; password_authn: $pw_rows"
        fi
        alice_uid=$(echo "$accounts_rows" | grep -oE '^user:[0-9]+=1$' | head -1 |
                    sed -E 's/^user:([0-9]+)=1$/\1/')
        if [ -n "$alice_uid" ] && \
           echo "$accounts_rows" | grep -q "^role:${alice_uid}=1$"; then
            note_pass "first user promoted to site-owner on disk"
        else
            note_fail "site-owner role not persisted for first user: $accounts_rows"
        fi
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
