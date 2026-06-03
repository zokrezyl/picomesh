# Generic service tooling: transport bridges and the `/_alpine` console

This document describes three framework-level, application-agnostic pieces
and keeps them clearly distinct from the picoforge browser UI. They were
added for inspecting and invoking the internal mesh without teaching any
backend to speak HTTP and without hardcoding picoforge routes (gh#15).

Four things that are easy to conflate — keep them separate:

| Thing | What it is | Source |
|---|---|---|
| **Picomesh transport frontend** | An inbound transport adapter (`yhttp`, `yrpc`, `yttp`, `cli`, `alpine`) selected per node with `frontend:`. | `src/picomesh/frontends/<name>/` |
| **Picoforge browser UI** | The product web app: HTML pages + static assets, sources data from the gateway. Knows picoforge routes. | `src/picoforge/webapp/` |
| **yrpc→yhttp transport bridge** | A plain `yhttp` node that exposes its configured remote yrpc services through the generic JSON API. Pure transport. No UI. | (config only) |
| **Generic `/_alpine` service console** | A separate `alpine` frontend that serves a static console and proxies it to a yhttp endpoint. No picoforge knowledge. | `src/picomesh/frontends/alpine/` |

## Correct layering

```text
backend service(s) over yrpc
  -> yrpc->yhttp bridge        (generic yhttp API: /_describe + JSON /_rpc)
      -> /_alpine console app   (static HTML/JS; talks only /_describe + /_rpc)
```

The bridge never serves `/_alpine`. The console never embeds a plugin route.
The picoforge webapp is unrelated to both.

## The generic yhttp service API

Any yhttp node that is **not** the gateway but has a non-empty `remotes:`
list is a transport bridge. It answers the same JSON surface the gateway
exposes, sourced from its **active configured services** (its local
`plugins:` plus its `remotes:`), never from the global class registry:

```text
GET|POST /_describe                      -> {"services":[{service,source,classes:[{class,qname,methods:[]}]}]}
GET|POST /<service.class>/_describe      -> {"path","class","methods":[]}
POST     /_rpc                           -> {"path":"service.class.method","args":[],"kwargs":{}}
```

Root `/_describe` lists services with a `source` (`local` / `remote`) and,
for each, its classes and method names. A bridge process registers exactly
the classes it activates locally plus the ones it proxies as remotes, so
the enrichment describes only the active service tree.

A bridge that fronts `session` becomes the **gateway** (auth boundary) and
also serves the picoforge auth/HTML POSTs. Keep `session` out of a pure
bridge's `remotes:` so it stays a transport bridge.

### Example bridge node

```yaml
mesh:
  services:
    internal_yhttp_bridge:
      host: 127.0.0.1
      port: 8230
      frontend: yhttp
      plugins: []
      config:
        remotes:
          - service: accounts
            port: 8204
          - service: git_repo
            port: 8209
          - service: issues
            port: 8208
```

Backends keep listening on `yrpc`; only the bridge listens on `yhttp`.

The bridge/console ports stay in the `8xxx` range deliberately: the perf
harness derives its mesh by a uniform `8xxx`→`9xxx` shift and relies on the
`9xxx` space being otherwise free, so these nodes must shift with the rest.

## The `/_alpine` service console

`/_alpine` is its own frontend (`frontend: alpine`,
`src/picomesh/frontends/alpine/`), started with configuration that points
it at a yhttp endpoint. It:

- serves the static console page at `GET /` and `GET /_alpine`,
- proxies `GET|POST /_describe`, `GET|POST /<path>/_describe`, and
  `POST /_rpc` to the configured upstream over yhttp.

The page is self-contained (no external scripts). It builds the whole UI
from `/_describe` and invokes through JSON `/_rpc`, so it works against any
yhttp-compatible endpoint: a bridge, the native gateway, or any future
HTTP-exposed service. It knows nothing about picoforge accounts, repos,
issues, or admin pages.

### Example console node

```yaml
mesh:
  services:
    service_console:
      host: 127.0.0.1
      port: 8231
      frontend: alpine
      config:
        alpine:
          upstream:
            host: 127.0.0.1
            port: 8230      # the bridge (or gateway) to proxy to — REQUIRED
          # token: "secret" # optional; required as ?token= or Bearer when set
```

Open `http://127.0.0.1:8231/_alpine` to browse and invoke the upstream's
services.

### Access control

The console can reach every service the upstream exposes, so access is
explicit by default:

- it is an opt-in node you configure (`frontend: alpine`),
- it binds `127.0.0.1` by default,
- setting `config.alpine.token` requires every request to carry the token
  (`Authorization: Bearer <token>` or `?token=<token>`); the page bootstraps
  the token from its own `?token=` query and replays it as a bearer header.

The console token gates the console only — it is never forwarded upstream.

## What this is not

- Not the picoforge browser UI (that is `picoforge-webapp`, a separate app).
- Not a way to make backends speak HTTP — they stay on `yrpc` behind a bridge.
- The binary `POST /_rpc?op=&id=` shim is internal transport compatibility
  for generated yrpc-style callers, not the console protocol. The console
  uses JSON `/_rpc` only.
