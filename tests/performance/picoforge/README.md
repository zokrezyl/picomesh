# picoforge performance harness (`picoforge-perf`)

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
- **Self-contained.** Plain C + pthreads + POSIX sockets; links no picomesh
  libraries. The same binary therefore benchmarks the picomesh C gateway *or*
  yaapp's Python gateway — the HTTP contract is identical.

## Build & run

Built as part of the normal build:

```sh
make build-desktop-release            # produces build-desktop-release/picoforge-perf
```

Bring the mesh up, then point the tool at it:

```sh
./tools/picoforge/mesh-up.sh &    # gateway on :8080
./build-desktop-release/picoforge-perf --scenario rpc_count --connections 16 --duration 10
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
| `mixed` | one weighted-random user action per iter | **realistic blended load** (all backends) |

### The `mixed` scenario + `make perf-picoforge`

`mixed` is the "lots of users doing lots of things" workload. Each worker
logs in as its own user and then issues a **weighted-random stream of real
actions** — `read_count`/`read_list` (reads), `put_file` (libgit2
commit), `open_issue`, `enqueue_run`, `make_repo` (bounded per worker), and
periodic re-`login` — emulating a live user session. Every action is a real
business call (no raw storage access); methods are
addressed over `/_rpc` carrying the session cookie, so the gateway
resolves sid→uid on every call (the real per-call cost). Two extra options:

| option | default | meaning |
|---|---|---|
| `--seed-users N` | 0 | stage 1: pre-create N account records (population scale) via `accounts.store.register` — O(1), bypasses the gateway user-name index so it scales to tens of thousands |
| `--repos-per-worker K` | 8 | cap on repos a worker creates (keeps total within the git_repo table) |

The turnkey way to run it is the Makefile target, which brings up an
**independent, port-shifted mesh** (every `8xxx`→`9xxx`, storage under
`/tmp/picoforge-perf`) so it never collides with a dev stack on the default
ports, runs the load, then tears the mesh down through its control parent:

```sh
make perf-picoforge                              # 50k seed, 32 workers, 60s
make perf-picoforge DURATION=30 CONNECTIONS=64   # override any of:
                                                 # SEED_USERS CONNECTIONS DURATION REPOS_PER_WORKER
```

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

## `mixed` load — measured (local, indicative)

`make perf-picoforge` with `SEED_USERS=50000 CONNECTIONS=32 DURATION=60`:

- **Seed:** 50,000 account records in ~7.5 s ≈ **6.7k creates/s** (0 errors).
- **Blended:** ~**14.4k ops/s**, 725k ok in 60 s. Latency mean 2.2 ms, p50 2.1,
  p90 2.8, p99 5.2, p99.9 7.7, max 21 ms.
- **By op:** reads (`read_count`/`read_list`), `login`, `enqueue_run`
  run clean (0 errors). 

Two findings the mixed load surfaces, both rooted in the inline/blocking design:

1. **Owner-checked writes (`put_file`) collapse under concurrency.** Clean at
   1–2 workers, ~60 % errors at 4, ~90 % at 16–32 — all `forbidden (not repo
   owner)`. The cause is the gateway resolving sid→uid (`session.store.lookup`,
   blocking) on every `/_rpc`: under contention it returns the wrong/zero uid,
   so the only owner-gated op (`put_file`) is rejected. Reads/`make`
   don't owner-check, so they don't expose it. This is the §5 blocking-rpc
   bottleneck, now quantified.
2. **The in-memory stores are fixed-size linear-scan tables.** `git_repo`
   (`REPOS_MAX`), `issues` (`ISSUES_MAX`) and `git_pipeline` (`PIPE_MAX`) were
   tiny (256/1024/256) and saturated instantly; raised to give load headroom,
   but a long run still fills `issues` — they need a real index to scale.
