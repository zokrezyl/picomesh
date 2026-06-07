# picomesh — architecture invariants

The picoforge app's assets live under `assets/picoforge/` (configs in
`config/`, web static files in `static/`) and its operator/build tooling
under `tools/picoforge/` (`mesh-up.sh`, `mesh-down.sh`, `smoke.sh`,
`deploy/`, `yemu/`). The layout mirrors yaapp's `git-yaapp` scenario.
These invariants apply throughout the repo. Anything that violates them is
a bug.

## How to ask me questions (assistant behavior)

- NEVER ask multiple-choice questions. Do NOT use the AskUserQuestion tool.
- Ask SHORT, plain-text questions. One thing at a time.

## Process lifecycle & teardown — the mesh owns its children

This is an absolute rule. Violating it wastes the user's time and is a bug.

- **NEVER `pkill`/`killall` or pattern-kill, and NEVER kill a spawned child
  directly** — not by PID, not by name, not even with the sandbox disabled.
  Reaching around the mesh to kill its processes is banned outright.
- **The mesh control parent OWNS the lifecycle of every child it spawned.**
  It spawned them (`mesh_store` via `uv_spawn`); it must take them down. The
  ONLY sanctioned way to stop the stack is to ask the parent to shut down —
  a control RPC, or `kill -TERM <parent-pid>` (the one process the operator
  started) — whereupon the mesh's own reaper SIGTERMs the children it
  started. Tooling signals the parent; the mesh kills the children.
- **Child PIDs are persisted in STORAGE, never a flat pidfile.** The mesh
  has the storage subsystem for exactly this. On bring-up the mesh first
  reaps any still-alive PIDs recorded by a previous run (clearing the
  port-in-use-on-restart races), then spawns fresh and records the new PIDs.
- **`mesh-up.sh` teardown goes through the mesh**, never `pkill`. If the
  stack must come down, drive it via the control surface.

## Routing must be service-driven, never hardcoded

- HTTP routes/pages must be derived from the ACTIVE services (registered
  classes/methods, local or remote) — discovered at runtime, not a
  hand-maintained `if (path_eq(...))` ladder.
- `picomesh` is the core: the mesh engine, plugins and the business logic.
  `picoforge` (`src/picoforge/`) is the app built on top of it.
- The gateway is API-only: `/_rpc`, `/_describe`, and the auth POSTs. It
  serves NO HTML pages and NO static files.
- The picoforge **webapp** (`src/picoforge/webapp/`, binary
  `picoforge-webapp`) is the ONLY web/page tier and the ONLY thing that
  serves static files; it sources everything from the gateway over `/_rpc`
  and the `/_describe` discovery surface. (The word "frontend" refers only
  to the engine's transport layer — `src/picomesh/frontends/`, the
  `--frontend yhttp|yrpc` selector — never to the webapp.)

## Process roles

- **Gateway** — the single HTTP endpoint of the mesh. It IS the
  authentication boundary.
  - Listens **yhttp only**, on a public port.
  - API-only: serves `/_rpc`, `/_describe`, and the auth/action POSTs
    (`/login`, `/register`, `/logout`, …) that mint/clear the session
    cookie. It serves NO HTML pages and NO static files — every GET HTML
    route 404s here; the webapp owns pages.
  - Exposes `POST /_rpc` for programmatic HTTP clients (tests, CLI).
  - Authenticates every incoming request via session cookie / bearer
    token, resolves to `uid`/`sid`.
  - Forwards RPCs to backend services over **yrpc** (binary) — never
    over HTTP.

- **Webapp** (`picoforge-webapp`) — the browser-facing page tier. Renders
  every HTML page and serves static assets, sourcing all data from the
  gateway over `/_rpc` + `/_describe`. Holds no plugins, no backend ports.

- **Backends** — every other service in the mesh (storage, accounts,
  session, password_authn, token_issuer, issues, git_repo,
  git_pipeline, personal_access_tokens, github_authn, portalloc).
  - Listen **yrpc only by default**. No HTTP. No HTML. No `/_rpc`.
  - Receive auth context as part of the yrpc body prefix (uid, sid).
  - Behind the gateway. Tests must NEVER hit a backend port directly.

- **Mesh control parent** — the bootstrap process that spawns the
  children via `mesh_store_reconcile_from_config`. Speaks yhttp on its
  control port for bootstrap-only RPC (`/create`, `/invoke` against
  `mesh_store`). Not part of the auth path; not what tests measure.

## Wire protocols

- Browser / curl ↔ **Gateway**: HTTP (HTML or JSON `/_rpc`).
- Gateway → Backend: **yrpc binary**. 8-byte caller-auth prefix
  (`uid` + `sid`) in the body; skel reads it before unpacking args.
- Backend → Backend: **yrpc binary** (same shape).

## Auth flow

### Token taxonomy — JWT is INTERNAL, opaque tokens are EXTERNAL

This distinction is load-bearing and gets violated easily. Read it twice.

- **JWT** — never crosses the mesh boundary. Lives only between the
  gateway and backends, encoded into the yrpc body prefix (`uid`/`sid`
  today; a richer claims payload tomorrow). No external client — not
  a browser, not curl, not an MCP server, not a CLI — ever sees a JWT.
  Not in headers, not in cookies, not in response bodies. If you find
  yourself about to write `Authorization: Bearer <jwt>` outside the
  mesh, stop: that is the bug.
- **Opaque bearer token** — what external clients use. Random,
  unguessable, no decodable structure. Issued by the gateway (via
  `token_issuer` / `personal_access_tokens`), stored server-side,
  presented by clients as `Authorization: Bearer <opaque>`.
- **Session cookie (`picomesh-sid`)** — the browser-facing equivalent of
  an opaque bearer. Same property: opaque, server-side lookup. The
  gateway accepts it either as a `Cookie:` header (normal browser POST)
  or as a `picomesh-sid:` header (cross-origin POSTs, programmatic
  clients that don't want cookies).

The gateway is the ONLY component that translates external → internal:
opaque bearer / sid → (lookup) → uid + claims → (mint or fetch) → JWT
for the yrpc body prefix. Backends only ever see the internal JWT
claims; they have no idea what an opaque token looks like.

### Login + call flow

1. Browser/tester POSTs to gateway `/login` with username + password.
2. Gateway authenticates via the `password_authn` backend over yrpc,
   mints a session via `session.start` over yrpc, returns a
   `picomesh-sid` cookie (opaque) to the browser. The JWT it minted to
   talk to backends during that login does NOT leave the gateway.
3. Subsequent browser requests carry the `picomesh-sid` cookie.
4. Programmatic clients (CLI, MCP, tests): present either
   `Cookie: picomesh-sid=<opaque>`, `picomesh-sid: <opaque>`, or
   `Authorization: Bearer <opaque>` to the gateway. All three are
   opaque-token forms — the gateway accepts any of them.
5. On every inbound request the gateway resolves the opaque token →
   `(uid, claims)` via `session.lookup` (yrpc), then forwards the
   downstream call with `(uid, sid)` in the yrpc body prefix. Backends
   trust the gateway's claim.

### MCP / external API surface

- The MCP server (and any other external automation in front of the
  gateway) calls `/_rpc` with `Authorization: Bearer <opaque>`. It
  never sees JWTs. Token issuance / revocation is the gateway's job.
- yaapp's frontend.py is the reference shape — note it uses the
  session cookie value as a header (`yaapp-session-id`), never a JWT.
  picomesh's gateway must mirror this: accept opaque tokens, reject any
  attempt to present a raw JWT.

## What NOT to do

- Do NOT make backends listen yhttp. The gateway is the only auth
  boundary; backends never touch HTTP. Adding HTTP parsing to a
  backend defeats the entire reason yrpc exists.
- Do NOT run tests against backend ports. The smoke MUST go through
  the gateway only, authenticated.
- Do NOT put auth in an HTTP request body. Headers / cookies only.
- Do NOT add new headers to the yrpc body. Headers are an HTTP-layer
  concept; tracing extensions ride at the gateway boundary, not the
  backend wire.
- Do NOT let a JWT leave the mesh. Not in `Authorization: Bearer`, not
  in cookies, not in `/_rpc` responses, not in error messages, not in
  logs surfaced to clients. JWTs are internal infrastructure. External
  clients get opaque tokens, full stop.
- Do NOT teach the gateway to accept a raw JWT from a client. If
  someone presents a JWT-shaped string as `Authorization: Bearer`,
  reject it — it's either a misconfigured client or an attacker who
  scraped a token from a log.

## Throughput notes

- yrpc on backends exists to minimize per-call CPU. Any change that
  introduces extra parsing on the backend hot path needs an explicit
  throughput justification.
- Throughput benchmarks measure gateway→backend yrpc round-trip cost
  specifically — that is the production hot path. The smoke is not
  a benchmark.

## Test invariant

`tools/picoforge/mesh-up.sh` exercises the gateway only. It must
POST to `/login`, capture the session cookie, then talk to the
gateway with that cookie. It must never reach a backend port
directly. If a test needs to invoke a backend method, it does so via
`POST /_rpc` on the gateway (which forwards over yrpc).
