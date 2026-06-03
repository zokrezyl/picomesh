#!/usr/bin/env bash
# Stop the picoforge mesh the RIGHT way.
#
# The mesh control parent OWNS every child it spawned. We do NOT pkill and
# we do NOT kill the children directly. Instead we read the parent's pid
# from the mesh's own storage (the `storage` plugin, context "mesh", key
# "parent" — recorded by mesh_mesh on reconcile) and send it SIGTERM. The
# parent's reaper then SIGTERMs the children it started.
#
# No pkill. No pidfile. The pid lives in storage because the mesh has
# storage for exactly this.
set -uo pipefail

DB="${1:-/tmp/picoforge/mesh-state.db}"
if [ ! -f "$DB" ]; then
    echo "mesh-down: no mesh state at $DB — nothing to stop."
    exit 0
fi

# Read kv_mesh.parent via python's built-in sqlite3 (no sqlite3 CLI dep).
pid=$(python3 - "$DB" <<'PY'
import sys, sqlite3
try:
    con = sqlite3.connect("file:%s?mode=ro" % sys.argv[1], uri=True)
    row = con.execute("SELECT v FROM kv_mesh WHERE k='parent'").fetchone()
    print((row[0] if row else "").strip())
except Exception:
    print("")
PY
)

if [ -z "$pid" ]; then
    echo "mesh-down: no parent pid recorded in storage — nothing to stop."
    exit 0
fi

if kill -0 "$pid" 2>/dev/null; then
    echo "mesh-down: SIGTERM control parent pid=$pid — its reaper stops the children."
    kill -TERM "$pid"
else
    echo "mesh-down: recorded parent pid=$pid is not alive (already stopped)."
fi
