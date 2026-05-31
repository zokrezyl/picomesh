#!/usr/bin/env bash
# Run the picoforge UI integration tests.
#
# Drives the real browser UI (Playwright + the system Google Chrome) through
# the full flow: sign up -> create repo -> create file -> edit file ->
# issue -> pipeline run -> sign out. By default it brings the whole mesh +
# webapp up itself against a fresh /tmp/picoforge and tears it down after.
#
# Prereqs:
#   make build-desktop-release      (builds picomesh + picoforge-webapp)
#   uv                              (https://docs.astral.sh/uv/)
#   google-chrome-stable            (Playwright uses it via channel="chrome")
#
# To drive an already-running stack instead of spawning one:
#   PICOFORGE_WEBAPP_URL=http://127.0.0.1:8081 ./run.sh
#
# Extra args are passed through to pytest, e.g. ./run.sh -k full_ui_flow -s
set -euo pipefail
cd "$(dirname "$0")"

# Playwright's `greenlet` C extension needs a modern libstdc++ on the loader
# path. On nix-provisioned toolchains the system one is too old / absent, so
# point LD_LIBRARY_PATH at the compiler's own libstdc++ (the one the project
# already builds against). Harmless elsewhere.
libstdcxx="$(gcc -print-file-name=libstdc++.so.6 2>/dev/null || true)"
if [ -f "$libstdcxx" ]; then
    export LD_LIBRARY_PATH="$(dirname "$libstdcxx")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

exec uv run --with pytest --with playwright pytest -v "$@"
