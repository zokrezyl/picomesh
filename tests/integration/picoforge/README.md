# picoforge UI integration tests

Browser-driven end-to-end tests for the picoforge **webapp** — they click
through the real UI exactly like a user and assert the rendered pages. Every
action round-trips **browser → `picoforge-webapp` → gateway `/_rpc` →
backends**; nothing here talks to a backend port directly.

## What's covered (`test_ui_flow.py`)

`test_full_ui_flow` walks the whole journey in one browser session:

1. **Sign up** via `/register` (falls back to `/login` if the account exists).
2. **Create a repository** from the `/repos` form.
3. See it **listed** and open the **repo browser**.
4. **Create a file** in the **Monaco editor** (path + content → Commit).
5. See the file in the **tree** and **open** it (content round-trips).
6. **Edit** the file and confirm the change persisted via `git_repo`.
7. **File an issue** and see the open count reflect it.
8. **Enqueue a pipeline run** and see the queued badge move.
9. **Sign out** back to `/login`.

`test_gateway_serves_no_html` cross-checks the invariant that the gateway is
API-only (HTML GETs 404 there; the webapp owns pages).

## Running

```sh
make build-desktop-release          # build picomesh + picoforge-webapp first
./run.sh                            # brings the stack up, runs, tears down
```

`run.sh` uses `uv run --with pytest --with playwright`, so no virtualenv setup
is needed. Playwright drives the **system Google Chrome** (`channel="chrome"`)
— no `playwright install` browser download required.

Pass pytest args through: `./run.sh -k full_ui_flow -s`.

### Driving an already-running stack

By default the suite spawns the control parent, reconciles the mesh, and
launches the webapp on `:8081` against a wiped `/tmp/picoforge`, then cleans
up. To instead drive a stack you already have running (e.g. a dev
`scenarios/picoforge/stack-up.sh`):

```sh
PICOFORGE_WEBAPP_URL=http://127.0.0.1:8081 ./run.sh
```

## Notes

- The tests `skip` (not fail) if the binaries aren't built, or if Playwright /
  Chrome can't launch — so CI without a browser degrades gracefully.
- Logs from the self-spawned stack land in `/tmp/picoforge/itest-*.log`.
