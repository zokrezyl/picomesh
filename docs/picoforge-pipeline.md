# Picoforge CI/CD — GitHub Actions pipelines (issue #33)

Picoforge's pipeline files use **GitHub Actions semantics**: a repository's
pipelines are `.github/workflows/*.yml` workflows, the same files GitHub runs.
(GitLab `.gitlab-ci.yml` semantics are planned as a second front end behind the
same job/descriptor model — see the roadmap.) The goal is to run real, basic
workflows — e.g. this repo's `build-3rdparty-*` workflows and the sibling
`../yetty` project's `cmake-multi-platform` workflow.

The architecture boundary is unchanged (CLAUDE.md): the webapp is browser-facing,
the gateway is the public API/auth boundary, and runners are **external clients
polling the gateway** — never inbound mesh services.

## The engine

`tools/picoforge/runner-agent/gha.py` is a self-contained GitHub Actions
workflow engine (parser + expression evaluator + executor). It runs standalone
and is also the runner-agent's executor.

```sh
# Inspect / dry-run
gha.py parse .github/workflows/build-3rdparty-libco.yml
gha.py jobs  .github/workflows/build-3rdparty-libco.yml --event push

# Execute a job from a checkout of THIS repo
gha.py run   .github/workflows/build-3rdparty-libco.yml \
             --event workflow_dispatch --repo "$PWD" --ref HEAD --job resolve
```

Exit code is 0 iff every executed job succeeded.

### Supported GitHub Actions subset

- **Triggers** — `on:` as a string, list, or mapping (the YAML 1.1 `on:`→`true`
  key gotcha is handled). Event-name match selects whether a workflow fires;
  branch/tag/path globs are advisory in the MVP.
- **Jobs** — `needs` (string or list) drives a topological order; a job whose
  dependency did not succeed is skipped. `runs-on` is recorded (informational).
- **Matrix** — `strategy.matrix` is expanded to the cartesian product of its
  list axes, honoring `include`/`exclude`; one job-run per combination.
- **Steps** — `run:` executes in `bash -eo pipefail` by default (or the step
  `shell`: `sh`/`python`/explicit). `uses: actions/checkout@*` clones the repo
  into the workspace at the requested ref/sha. `working-directory`, per-step and
  per-job `env`, and step `id` are honored.
- **The GitHub environment contract** — every step gets `GITHUB_WORKSPACE`,
  `GITHUB_SHA`, `GITHUB_REF`, `GITHUB_REF_NAME`, `GITHUB_EVENT_NAME`,
  `GITHUB_REPOSITORY`, `GITHUB_OUTPUT`, `GITHUB_ENV`, `GITHUB_PATH`,
  `GITHUB_STEP_SUMMARY`, `RUNNER_OS`, `RUNNER_TEMP`, `CI`, `GITHUB_ACTIONS`.
  Writing `name=value` (or the `name<<EOF` heredoc) to `$GITHUB_OUTPUT` becomes
  `steps.<id>.outputs.<name>`; `$GITHUB_ENV` is merged into the env of later
  steps; `$GITHUB_PATH` is prepended to `PATH`. Job `outputs:` are resolved from
  the step context and exposed to dependents as `needs.<job>.outputs.<name>`.
- **Expressions** — `${{ … }}` over the `github`, `matrix`, `env`, `steps`,
  `needs`, `runner`, `strategy`, `job` contexts, with the operators
  `== != > < >= <= && || !` and the functions `format`, `contains`,
  `startsWith`, `endsWith`, `join`, `toJSON`, `fromJSON`, `success`, `failure`,
  `always`, `cancelled` (`hashFiles` returns "" in the MVP). `if:` on jobs and
  steps is evaluated (wrapped `${{ }}` or bare).
- **Failure semantics** — a failing step fails the job; later steps are skipped
  unless gated by an `if:` that uses `failure()`/`always()`.

### Not supported (skipped, never silently passed)

- Marketplace `uses:` actions other than `actions/checkout` are **logged as
  SKIPPED** — the `run:` steps still execute. So a workflow that relies on, e.g.,
  `softprops/action-gh-release` to upload assets will run its build steps and
  skip the upload, rather than failing or pretending success. (Roadmap: a
  pluggable action-shim registry for the common actions.)
- Container/service jobs, reusable workflows (job-level `uses:`), `concurrency`,
  `permissions`, and real secret injection (secrets resolve to empty).
- Branch/tag/path trigger filtering, caching (`actions/cache`), and artifact
  upload/download actions.

A workflow only runs green to completion if the runner host actually has the
toolchain its `run:` steps need (compilers, `apt`, …); the engine provides the
GitHub-Actions execution *semantics*, not an Ubuntu image.

## Runner integration

The runner-agent (`runner-agent.py`) detects a workflow job and executes it
through the engine, streaming every line to `git_pipeline.append_log`. A leased
job descriptor is treated as a workflow when it carries `workflow_content` (the
YAML text) or a `pipeline_path` under `.github/workflows`. Descriptor fields the
runner consumes:

| field | meaning |
|---|---|
| `workflow_content` | the workflow YAML (read by the mesh from the repo at the ref) |
| `pipeline_path` | fallback path to the workflow file (dev/local shared disk) |
| `repo_url` / `repo_path` | clone source for `actions/checkout` |
| `sha`, `ref`, `event` | the triggering commit / ref / event name |
| `job` | optional single job to run (else all jobs) |

Non-workflow descriptors still run the legacy shell stub, so existing smoke
flows are unaffected.

## Execution-ready job descriptor (mesh side)

`git_pipeline.lease_job` returns an immutable descriptor authenticated by the
runner token. Current fields: `job_id, repo_id, runner_id, status, ref,
pipeline_path, attempt, timeout, lease_expiry`. `attempt` is the run-attempt
counter (incremented by `requeue_expired`).

**Remaining mesh wiring to make workflow runs fully mesh-driven** (tracked):
`lease_job` should also populate `workflow_content` (via `git_repo.read_file` at
the ref), the resolved `sha` (a `git_repo` ref→SHA resolver), a `repo_url`/
`repo_path` clone source, and the `event`. Until then, workflows run via the
standalone engine CLI against a local checkout, and the runner runs a workflow
the moment a descriptor carries those fields.

## Runner authorization (enforced today)

Runner identity is the verified token JWT (`runner:<id>` + the runner group);
every lifecycle method is gated on it (no `uid==0` / client-supplied-id bypass):
`lease`/`lease_job` require the caller's token to match the `runner_id`;
`append_log`/`complete_job`/`read_log` require the caller to actively own the
job's lease. Transitions are single atomic SQL UPDATEs (double-complete / racing
requeue affect zero rows the second time).

## Roadmap status

Landed:

- [x] GitHub Actions engine: parser, expression evaluator, matrix, `needs`,
      `run:`/`checkout`, `$GITHUB_OUTPUT`/`$GITHUB_ENV`/`$GITHUB_PATH`, job
      outputs, `if:` — runs this repo's and ../yetty's workflows' `run:` steps.
- [x] Runner-agent executes a workflow descriptor through the engine, streaming
      logs to `append_log`.
- [x] `attempt` run-counter in the data model + descriptor.
- [x] Runner authorization (owner-gated lease/append/complete).

Tracked / sequenced:

1. Mesh descriptor enrichment: `workflow_content` + resolved `sha` + clone
   source + `event` (so a queued run is self-contained).
2. Pluggable `uses:` action shims (checkout exists; add cache, upload-artifact,
   gh-release, setup-* as needed).
3. Push/tag-triggered enqueue (read `.github/workflows/*` on push, match `on:`,
   enqueue the matching workflow); then scheduled and PR triggers.
4. Isolated container execution + an Ubuntu-like toolchain image.
5. Artifact storage (gateway-mediated chunked upload/download).
6. run/job/step data model for matrix fan-out + retries; lease-time label
   matching; runner admin UI; deployment environments; CI observability.
7. GitLab `.gitlab-ci.yml` front end behind the same descriptor model.
