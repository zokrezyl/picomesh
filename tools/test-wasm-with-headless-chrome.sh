#!/usr/bin/env bash
#
# test-wasm-with-headless-chrome.sh — end-to-end test of the in-browser
# wasm demo, driven through a real headless browser.
#
# Why a real browser (under xvfb) and not a native mesh run:
#   The demo is a RISC-V emulator compiled to wasm. It only makes
#   forward progress when the page's requestAnimationFrame fires. A
#   truly headless Chrome aggressively throttles rAF / background
#   timers, so the emulator never boots. Running a *visible* Chrome on
#   a virtual X server (xvfb) keeps rAF ticking at full rate while
#   still needing no physical display.
#
# What it does:
#   1. Serves the wasm bundle over HTTP (python serve.py).
#   2. Launches Chrome (under xvfb) pointed at tinyemu-iframe.html.
#   3. Drives the page over the Chrome DevTools Protocol exactly like a
#      user would: boots the in-VM gateway, registers an account,
#      creates a repo, opens the repo page, lists the account's repos.
#   4. Asserts on the *rendered HTML*, not just HTTP status codes — a
#      200 can still be an error page.
#
# Exit code: 0 = every step passed, 1 = a step failed (details in the
# log). Output goes to tmp/test-wasm-headless.log under the repo root.
#
# Usage:
#   tools/test-wasm-with-headless-chrome.sh [bundle_dir] [http_port]
#
# Env knobs:
#   BOOT_WAIT_S   max seconds to wait for the in-VM gateway (default 300)
#   KEEP_SERVER   set to 1 to leave the HTTP server running on exit

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUNDLE_DIR="${1:-build-webasm-yemu-release}"
HTTP_PORT="${2:-8090}"
BOOT_WAIT_S="${BOOT_WAIT_S:-300}"
LOG=tmp/test-wasm-headless.log

mkdir -p tmp

die() { echo "FATAL: $*" >&2; exit 2; }

[[ -f "$BUNDLE_DIR/tinyemu-iframe.html" ]] || \
    die "no wasm bundle at $BUNDLE_DIR (build with: make build-webasm-yemu-release)"
[[ -f "$BUNDLE_DIR/serve.py" ]] || die "no serve.py in $BUNDLE_DIR"
command -v node        >/dev/null || die "node not on PATH"
command -v google-chrome >/dev/null || die "google-chrome not on PATH"
command -v xvfb-run    >/dev/null || die "xvfb-run not on PATH (apt install xvfb)"
command -v python3     >/dev/null || die "python3 not on PATH"

DRIVER="$REPO_ROOT/tools/test-wasm-with-headless-chrome.mjs"
[[ -f "$DRIVER" ]] || die "driver missing: $DRIVER"

echo "== wasm headless browser test =="
echo "   bundle : $BUNDLE_DIR"
echo "   port   : $HTTP_PORT"
echo "   log    : $LOG"

# --- clean any stragglers from a previous run --------------------------
pkill -9 -f 'remote-debugging-port=9222' 2>/dev/null || true
pkill -9 Xvfb 2>/dev/null || true
rm -rf /tmp/test-wasm-chrome-profile
: > "$LOG"

# --- HTTP server -------------------------------------------------------
SERVER_PID=""
if ss -ltn 2>/dev/null | grep -q ":${HTTP_PORT}\b"; then
    echo "   (reusing HTTP server already on :$HTTP_PORT)"
else
    setsid python3 "$BUNDLE_DIR/serve.py" "$HTTP_PORT" "$BUNDLE_DIR" \
        >> "$LOG" 2>&1 &
    SERVER_PID=$!
    sleep 1
    ss -ltn 2>/dev/null | grep -q ":${HTTP_PORT}\b" || \
        die "HTTP server failed to bind :$HTTP_PORT (see $LOG)"
    echo "   started HTTP server pid=$SERVER_PID"
fi

cleanup() {
    pkill -9 -f 'remote-debugging-port=9222' 2>/dev/null || true
    pkill -9 Xvfb 2>/dev/null || true
    if [[ -n "$SERVER_PID" && "${KEEP_SERVER:-0}" != "1" ]]; then
        kill -9 "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# --- drive the browser -------------------------------------------------
echo "   booting VM + driving (up to ${BOOT_WAIT_S}s for gateway)…"
set +e
BOOT_WAIT_S="$BOOT_WAIT_S" \
HTTP_PORT="$HTTP_PORT" \
xvfb-run -a node "$DRIVER" 2>&1 | tee -a "$LOG"
RC=${PIPESTATUS[0]}
set -e

echo
if [[ $RC -eq 0 ]]; then
    echo "RESULT: PASS — full demo works in the browser (see $LOG)"
else
    echo "RESULT: FAIL (rc=$RC) — see $LOG"
fi
exit $RC
