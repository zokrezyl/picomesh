# picoforge integration tests

End-to-end tests for the picoforge stack. Two complementary suites:

- **`test_ui_flow.py`** — browser-driven: clicks through the real webapp UI
  exactly like a user and asserts the rendered pages.
- **`test_mesh_correctness.py`** — mesh correctness: a service-driven sweep of
  the gateway's reflected RPC surface plus the admin roster page.

Every action round-trips **browser/curl → gateway `/_rpc` → backends** (or
**browser → `picoforge-webapp` → gateway → backends**); nothing here talks to
a backend port directly.

## Mesh correctness (`test_mesh_correctness.py`)

These exist because a renamed RPC path (`accounts.store.count` →
`accounts.accounts.count`) once left the admin **Users** page silently showing
"accounts service unreachable" — the UI-flow suite never opened that page, so
nothing caught it.

- `test_gateway_describe_lists_core_services` — `/_describe` reflects every
  core service (a missing one == degraded bring-up).
- `test_gateway_rpc_surface_reachable` — **service-driven**: discovers every
  service/class/method from `/_describe` (no hardcoded ladder), reflects each
  global read method's parameters from `/<path>/_describe`, invokes it over
  `/_rpc` with benign default args, and asserts a real result. A renamed,
  dropped, or crashing `count`/`list`/`list_all` fails here.
- `test_accounts_roster_rpc` / `test_accounts_list_pagination` — the exact
  calls the admin page makes (count + list_all agree; `list` is capped,
  `list_all` is the full set).
- `test_distributed_trace_reconstructs` — a traced request through the gateway
  produces a linked, cross-service span tree in the `trace_collector` (guards
  the span exporter; a renamed ingest method silently drops every span).
- `test_admin_users_roster` / `..._count_tile` — drives the literal admin page
  through the browser and asserts the account table renders.

## What the UI flow covers (`test_ui_flow.py`)

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

Pass pytest args through: `./run.sh -k mesh or ./run.sh -k full_ui_flow -s`.
The RPC-surface tests (`-k "rpc or roster_rpc or pagination or core_services"`)
need no browser, so they run even where Chrome is unavailable.

## Lifecycle (no pkill)

The suite spawns the control parent, reconciles the mesh (which spawns the
gateway + backends), and launches the webapp, against a wiped `/tmp/picoforge`.
Teardown is the **mesh-sanctioned** path: the control parent owns the backend
children it spawned, so the harness SIGTERMs the parent (its own `Popen`
handle, and `tools/picoforge/mesh-down.sh` for a stale prior run) and the
parent's reaper stops the children. The webapp is a standalone sidecar the
harness owns and terminates via its own handle. Nothing is `pkill`ed and no
backend port is signalled directly. The webapp port defaults to `:8080` but the
harness **auto-picks a free port** if that one is busy, so a leftover sidecar
never blocks a run.

### Driving an already-running stack

To instead drive a stack you already have running (e.g. a dev
`tools/picoforge/mesh-up.sh`):

```sh
PICOFORGE_WEBAPP_URL=http://127.0.0.1:8080 ./run.sh
# point the RPC tests at a non-default gateway with PICOFORGE_GATEWAY_URL
```

## Notes

- The tests `skip` (not fail) if the binaries aren't built, or if Playwright /
  Chrome can't launch — so CI without a browser degrades gracefully.
- Logs from the self-spawned stack land in `/tmp/picoforge/itest-*.log`.
