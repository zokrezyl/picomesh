#!/usr/bin/env bash
# msgpack frontend smoke: bring up an ISOLATED collocated picomesh on a free
# port with the calculator plugin and the `msgpack` frontend, then drive it
# through the reference Python client. Self-contained — the server is reaped
# by `timeout`, nothing is killed by pid, no shared storage/ports.
#
# Run from the repo root after `make build-desktop-release`.

set -uo pipefail

# Error-only tracing on by default (inherited by the spawned server).
# Override: YTRACE_LOG_LEVEL=trace for full tracing, YTRACE_DEFAULT_ON=no to mute.
export YTRACE_DEFAULT_ON="${YTRACE_DEFAULT_ON:-yes}"
export YTRACE_LOG_LEVEL="${YTRACE_LOG_LEVEL:-error}"

PICOMESH=./build-desktop-release/picomesh
CLIENT=./tools/msgpack-client/picomesh_msgpack.py
HOST=127.0.0.1
PORT=7911
LIFETIME=25

mkdir -p tmp
PASS=0
FAIL=0
pass() { PASS=$((PASS+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); echo "  FAIL: $1  --  got: $2" >&2; }

# expect <label> <expected-substring> <actual>
expect() { case "$3" in *"$2"*) pass "$1" ;; *) fail "$1" "$3" ;; esac; }

echo "[1/2] starting picomesh --frontend msgpack --plugins calculator on :$PORT"
timeout "$LIFETIME" "$PICOMESH" --plugins calculator \
    --frontend msgpack --host "$HOST" --port "$PORT" serve > tmp/msgpack-srv.log 2>&1 &
SRV=$!

# Wait for the port to accept (the server logs nothing at default verbosity).
up=0
for _ in $(seq 1 60); do
    if (exec 3<>"/dev/tcp/$HOST/$PORT") 2>/dev/null; then exec 3>&- 3<&-; up=1; break; fi
    sleep 0.25
done
[ "$up" = 1 ] && pass "server accepts on :$PORT" || { fail "server never came up" "see tmp/msgpack-srv.log"; }

echo "[2/2] driving the reference client"

out=$("$CLIENT" invoke "$HOST" "$PORT" calculator.calc.add '[2,3]' 2>&1)
expect "invoke calculator.calc.add(2,3) == 5" '"result": 5' "$out"

out=$("$CLIENT" invoke "$HOST" "$PORT" calculator.calc.mul '[6,7]' 2>&1)
expect "invoke calculator.calc.mul(6,7) == 42" '"result": 42' "$out"

# GATE: an inactive service must be rejected before any class/method lookup.
out=$("$CLIENT" invoke "$HOST" "$PORT" nope.calc.add '[1,2]' 2>&1)
expect "gate: inactive service rejected" '"code": "service_not_active"' "$out"

# A registered-but-wrong method on an active service -> no_such_method.
out=$("$CLIENT" invoke "$HOST" "$PORT" calculator.calc.bogus '[1,2]' 2>&1)
expect "unknown method rejected" '"code": "no_such_method"' "$out"

# v1 is positional-only: non-empty kwargs is rejected (not silently ignored).
out=$("$CLIENT" invoke "$HOST" "$PORT" calculator.calc.add '[2,3]' --kwargs '{"z":1}' 2>&1)
expect "non-empty kwargs rejected" '"code": "kwargs_unsupported"' "$out"

# Per-type validation: passing strings where ints are expected -> call_error.
out=$("$CLIENT" invoke "$HOST" "$PORT" calculator.calc.add '["a","b"]' 2>&1)
expect "bad arg type rejected" '"code": "call_error"' "$out"

# Framing: a length prefix over the 1 MiB cap -> frame_too_large.
out=$("$CLIENT" oversize "$HOST" "$PORT" 2>&1)
expect "oversize frame rejected" '"code": "frame_too_large"' "$out"

# describe reflects the positional parameter signature.
out=$("$CLIENT" describe "$HOST" "$PORT" calculator.calc.add 2>&1)
expect "describe lists params x,y" '"name": "x"' "$out"

# describe is gated too: an inactive service is rejected, not reflected.
out=$("$CLIENT" describe "$HOST" "$PORT" nope.calc.add 2>&1)
expect "describe: inactive service rejected (gated)" '"code": "service_not_active"' "$out"

echo
echo "========================================"
echo "PASS: $PASS    FAIL: $FAIL"
echo "========================================"
wait "$SRV" 2>/dev/null
[ "$FAIL" -eq 0 ]
