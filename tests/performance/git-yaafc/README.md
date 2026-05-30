# git-yaafc performance harness (`yaafc-perf`)

A load generator that drives the **gateway** (the production hot path) and
reports latency percentiles + throughput under concurrency. It is the
measurement counterpart to issue #4 §5.

## Design

- **Gateway only.** It never touches a backend port directly — same invariant
  as `mesh-up.sh`. Auth goes through `/login`; calls go through `/_rpc` or the
  HTML form routes, exactly as a real client would.
- **Real threads, real sockets.** Each `--connections` worker is an OS thread
  with its own keep-alive TCP connection doing blocking request/response
  round-trips. Independent threads give honest concurrency and per-request
  latency without sharing — or being throttled by — the gateway's event loop.
- **Self-contained.** Plain C + pthreads + POSIX sockets; links no yaafc
  libraries. The same binary therefore benchmarks the yaafc C gateway *or*
  yaapp's Python gateway — the HTTP contract is identical.

## Build & run

Built as part of the normal build:

```sh
make build-desktop-release            # produces build-desktop-release/yaafc-perf
```

Bring the mesh up, then point the tool at it:

```sh
./scenarios/git-yaafc/mesh-up.sh &    # gateway on :8080
./build-desktop-release/yaafc-perf --scenario rpc_count --connections 16 --duration 10
```

### Options

| option | default | meaning |
|---|---|---|
| `--host H` | `127.0.0.1` | gateway host |
| `--port P` | `8080` | gateway port |
| `--connections N` | `8` | concurrent worker threads |
| `--duration SECS` | `10` | run for this long |
| `--requests R` | — | fixed requests **per worker** (overrides `--duration`) |
| `--scenario NAME` | `rpc_count` | which workload (below) |

### Scenarios

| name | what it times (per iteration) | exercises |
|---|---|---|
| `rpc_count` | `POST /_rpc git_repo.store.count_total` | read; gateway → 1 backend |
| `login` | `POST /login` | auth composite (4 backends) |
| `repo_create` | `POST /repos/new` (authenticated) | write; storage + git_repo |
| `register` | `POST /register` (new user each time) | write; accounts + authn |
| `full` | register → login → repo create → list | end-to-end user journey |

Each worker establishes a session (register + login) before the timed loop for
the scenarios that need one; `register`/`full` mint fresh users per iteration.

### Output

```
scenario       : rpc_count
connections    : 16 (16 sessions established)
wall time      : 4.004 s
requests       : 11226 ok, 0 errors
throughput     : 2803.6 req/s
latency (ms)   : mean=5.352 min=1.896 p50=4.064 p90=7.913 p99=31.250 p99.9=63.656 max=80.087
```

Exit status is non-zero if any request errored or none succeeded — usable in CI
gates once the runtime is fast enough to set thresholds. "sessions established"
< connections is a red flag that results for a session-bound scenario are not
trustworthy (the gateway/backends buckled under setup load).

## Extending

Add a `case` in `scenario_step()` and a row in `scenario_lookup()`'s
`SCENARIOS` table. Keep each step one timed unit of work so the percentiles
stay meaningful. Future additions: issues open/close, pipeline enqueue/lease,
PAT mint, concurrent-session fan-out, delayed-backend stress.

## Early findings (baseline, blocking runtime — pre §5 async work)

Indicative local numbers at 16 connections (they vary run to run — treat as a
baseline to beat, not a target):

| scenario | throughput | p50 | p99 |
|---|---|---|---|
| `repo_create` (write) | ~12k req/s | ~1.0 ms | ~3.5 ms |
| `login` (auth, 4 backends) | ~590 req/s | ~22 ms | ~54 ms |
| `register` (write) | ~100 req/s | ~136 ms | ~263 ms |

What stands out, and motivates the §5 async track:

- **`/_rpc` is round-trip heavy.** Each call costs ~6 *blocking* gateway→backend
  round-trips: opaque-token → uid resolution alone is `session` create + lookup
  + destroy, then the call is object create + invoke + destroy. Obvious wins:
  cache the token→uid resolution, and reuse a per-session backend object instead
  of create/destroy per call.
- **Auth composite (`login`) and `register` are an order of magnitude slower**
  than a single write — they serialise several backend round-trips on the
  blocking `rpc_call` path.
- **The mesh is slow to recover from a load burst.** Right after a heavy run,
  session establishment for the *next* scenario can fail wholesale — the harness
  reports it honestly as "sessions established" dropping below `--connections`
  and then `0 requests` (it bails rather than spin). This is the head-of-line
  blocking / blocking-`rpc_call` / inline-storage behaviour §5 calls out. The
  harness inserts settle time between scenarios to give the mesh a chance to
  drain; the fact that it needs to is itself the finding.
