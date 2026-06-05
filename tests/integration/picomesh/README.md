# picomesh per-plugin isolation throughput tests

Each test runs **one** picomesh process with **only** the plugin(s) under test —
no gateway, no session/auth tier, no other services — and hammers it directly to
find that plugin's raw throughput ceiling. This isolates "is the bottleneck in
*this* service?" from the mesh's gateway round-trips and cross-service hops.

```sh
make build-desktop-release
bash tests/integration/picomesh/relational_storage/run.sh   # SQLite relational store
bash tests/integration/picomesh/git_repo/run.sh             # libgit2 commits
```

Knobs (env): `CONNS` (concurrent client processes), `DURATION`, `WORKERS`,
`SHARDS`, `PORT`. Defaults approximate saturation; sweep `CONNS` to map scaling:

```sh
for c in 8 24 48; do CONNS=$c DURATION=10 bash .../relational_storage/run.sh; done
```

The driver spawns `CONNS` worker **processes** (real parallelism — a single
Python process is GIL-bound and undercounts) and reports ops/s split by
write/read, plus error % and mean latency.

## How they reach the plugin

- **relational_storage** — driven through the **stateless path `/_rpc`** (the
  node is put in *bridge* mode via a dummy remote, so it does object
  create+invoke+release per call, the same shape the mesh uses internally). No
  auth: relational_storage has no owner checks. Uses picomesh's **real product
  schema** (mirror of `rstore_uid` in `assets/picoforge/config/picoforge.yaml`).
- **git_repo** — `put_file` is owner-gated on a **verified JWT**, so the node
  runs in gateway mode with a `bearer_jwt_token` authenticator + permissive
  authorizer, and the driver **mints a `site:owner` JWT** (HS256, the node's
  test secret) to exercise the real owner-checked libgit2 commit path. Each
  worker commits to its own bare repo (parallel across repos).

## Measured ceilings (24-core box, workers:4, shards:8)

| plugin | ops/s | writes/commits per s | reads/s | bound by |
|---|---|---|---|---|
| relational_storage | ~12k | ~6k writes/s | ~6k | SQLite per-shard write txn + fsync |
| git_repo | ~16k | ~8k commits/s | ~8k | libgit2 zlib + SHA on the libuv pool |

relational_storage's **write** ceiling (~6k/s) is the lower of the two — and it
sits on the mesh's per-request path (every request resolves the session via a
relational SELECT), which is why caching token→uid is the top throughput lever.
