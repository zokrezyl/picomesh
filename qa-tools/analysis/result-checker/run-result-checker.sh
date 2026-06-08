#!/bin/bash
# Run result-checker on all picomesh C files.
#
# Any extra arguments are forwarded to the checker, so the second mode can be
# enabled per invocation:
#
#   ./run-result-checker.sh                                   # propagation check (default)
#   ./run-result-checker.sh --check-double-eval               # both checks
#   ./run-result-checker.sh --check-double-eval --check-propagation=false  # double-eval only

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PICOMESH_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# BUILD_DIR supplies the compilation database (compile_commands.json) and the
# per-file compile flags via `-p`. It is the real picomesh build.
BUILD_DIR="${BUILD_DIR:-${PICOMESH_ROOT}/build-desktop-release}"

# The checker itself lives in its own host-compiled tree — see the Makefile
# `result-checker` target for why it must NOT be the nix-compiled main build.
CHECKER="${PICOMESH_ROOT}/build-qa-tools/result-checker/result-checker"

if [ ! -x "$CHECKER" ]; then
    echo "result-checker not built — building it (host toolchain)…"
    make -C "$PICOMESH_ROOT" result-checker || {
        echo "Error: failed to build result-checker"
        exit 1
    }
fi

if [ ! -f "${BUILD_DIR}/compile_commands.json" ]; then
    echo "Error: compile_commands.json not found"
    exit 1
fi

# Only feed the checker files that actually have an entry in the compilation
# database. The src/picomesh tree carries .c files that are never compiled in a
# given build (platform-specific sources, plus *.gen.c units that are #included
# into a parent rather than compiled standalone). With no compile_commands.json
# entry, LibTooling falls back to a flagless guess and drowns the real
# diagnostics in "fatal error: '<x>.h' file not found" noise. Intersect the
# source list with the database so each file is analysed with its recorded
# compile flags, and skipped silently when it has none.
mapfile -t source_files < <(
    python3 - "${BUILD_DIR}/compile_commands.json" "${PICOMESH_ROOT}/src" <<'PYEOF'
import json, os, sys

database_path, source_root = sys.argv[1], sys.argv[2]

with open(database_path) as database_file:
    compiled = {os.path.realpath(entry["file"]) for entry in json.load(database_file)}

for current_dir, _subdirs, names in os.walk(source_root):
    for name in names:
        if not name.endswith(".c"):
            continue
        path = os.path.join(current_dir, name)
        if os.path.realpath(path) in compiled:
            print(path)
PYEOF
)

if [ "${#source_files[@]}" -eq 0 ]; then
    echo "Error: no source files matched compile_commands.json entries"
    exit 1
fi

printf '%s\n' "${source_files[@]}" | \
    xargs "$CHECKER" -p "$BUILD_DIR" "$@" 2>&1
