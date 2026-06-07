# picoforge browser page tests (`playwright/`)

Real Playwright + Python tests that drive the picoforge **webapp** in a browser
(system Google Chrome via `channel="chrome"`), covering the pages the
top-level `test_ui_flow.py` happy path does not open. They share the same
session stack and fixtures (`page`, `base_url`, `gateway_url`) defined in the
parent `tests/integration/picoforge/conftest.py`, and the same `helpers.py`
sign-in/register primitives. Everything round-trips
**browser → picoforge-webapp → gateway `/_rpc` → backends**; nothing here
talks to a backend port directly.

## Files

- **`test_shell_nav.py`** — the shared app chrome: topbar (brand, global
  search, New/Admin actions), the Projects/Groups/Issues/Pipelines sidebar nav,
  and global search (match, no-match, and submitting the topbar box).
- **`test_dashboards.py`** — the cross-repo dashboards `/-/dashboard/issues`
  and `/-/dashboard/runs` (RBAC repo discovery + the global pipeline counters).
- **`test_project_pages.py`** — the project surfaces the journey skips: the
  Settings tab, the bare-path `/​<namespace>` account landing, and the
  Issues/Pipelines action forms (close-by-id, lease-a-job).
- **`test_groups.py`** — the GitLab-style group UI: the `/-/groups` list,
  creating a top-level group, and the detail page's member grant/revoke flow.
  (RBAC at the RPC layer is `test_rbac.py`; this is the operator's UI view.)
- **`test_admin_pages.py`** — the admin area beyond Users: Overview,
  Repositories, Tokens (incl. the admin-only PAT mint relay), Services (the
  live `/_describe` roster), Namespaces, and a namespace's member detail page.
- **`test_auth_lifecycle.py`** — the auth negative paths: login/register page
  shapes, wrong-password and duplicate-username errors, and that logout really
  clears the session (a protected page bounces afterwards).
- **`test_repo_files.py`** — file-tree navigation: the empty-repo state,
  creating a file in a subdirectory, descending/`..`-ascending the tree, the
  dir-scoped New-file button, and the editor's read-only vs editable path.
- **`test_rbac_ui.py`** — namespace RBAC with **two concurrent sessions**
  (`open_second_user`): repo-list and search isolation, a non-maintainer
  forbidden from a group's detail page, an unknown-user grant surfacing an
  error, and subgroup creation showing up in the admin index.
- **`test_negative_routes.py`** — unknown commands/verbs 404, anonymous
  visitors bounce from protected pages, anonymous mutations are refused, and
  the issues dashboard leaks no repos to a signed-out caller.
- **`test_dashboard_consistency.py`** — cross-page: a filed issue and an
  enqueued run each show up on the mesh-wide dashboard (the aggregate read
  sees the write).

`forge.py` adds idempotent helpers (`ensure_repo`, `ensure_group`,
`file_an_issue`) plus `open_second_user`, which spins up a second browser
context with its own cookie jar for the multi-user RBAC tests.

## Running

```sh
make build-desktop-release                     # picomesh + picoforge-webapp
PICOMESH_JWT_SECRET=<any-non-empty-secret> \
  ./run.sh playwright/                          # brings the stack up + tears down
```

The stack inherits `PICOMESH_JWT_SECRET` from the environment (the config sets
`security.jwt_secret: "${PICOMESH_JWT_SECRET}"`). With it unset, every
login/register silently bounces back to `/-/login` and the whole suite fails at
sign-in — set it before running, exactly as for the other session-stack suites.

A note on group membership: a group created mid-session only appears in the
user's `/-/groups` list **after re-authenticating** — the namespace claims are
snapshotted into the access token at login (`route_whoami`), so the create test
logs in again to pick up the freshly persisted owner membership.
