#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyyaml"]
# ///
"""Picoforge GitHub Actions workflow engine (issue #33).

Picoforge speaks **GitHub Actions semantics** for its pipeline files (GitLab
semantics come later). This module parses a `.github/workflows/*.yml`, expands
the matrix, orders jobs by `needs`, and executes each job's steps from a clean
checkout — enough to actually run the `run:`-step parts of real workflows like
this repo's `build-3rdparty-*` and ../yetty's `cmake-multi-platform`.

What is supported
-----------------
- `on:` event filter (push/tag/workflow_dispatch; the YAML `on:`→True key gotcha
  is handled), `jobs`, `needs` (topological order), `strategy.matrix` (cartesian
  product + `include`/`exclude`), `runs-on` (informational), per-job/step `env`,
  `if:` (expression), job `outputs`.
- Steps: `run:` (executed in `bash` by default, or the step `shell`), and
  `uses: actions/checkout@*` (git clone of the repo into the workspace at the
  requested ref/sha). The GitHub env contract is provided: `GITHUB_WORKSPACE`,
  `GITHUB_SHA`, `GITHUB_REF`, `GITHUB_REF_NAME`, `GITHUB_EVENT_NAME`,
  `GITHUB_REPOSITORY`, `GITHUB_OUTPUT`, `GITHUB_ENV`, `GITHUB_PATH`,
  `GITHUB_STEP_SUMMARY`, `RUNNER_OS`, `CI`. `$GITHUB_OUTPUT` becomes the step's
  outputs (`steps.<id>.outputs.*`); `$GITHUB_ENV` is merged into the job env for
  later steps; `$GITHUB_PATH` is prepended to PATH.
- `${{ }}` expressions over the `github`, `matrix`, `env`, `steps`, `needs`,
  `runner`, `strategy`, `job` contexts, with `format/contains/startsWith/`
  `endsWith/join/fromJSON/toJSON/success/failure/always/cancelled` and the
  `== != && || !` operators.

What is NOT supported (logged + skipped, never silently passed)
---------------------------------------------------------------
- Marketplace `uses:` actions other than `actions/checkout` — the step is logged
  as SKIPPED (unsupported). A workflow that depends on, e.g.,
  `softprops/action-gh-release` to "succeed" will have that step skipped; the
  `run:` steps still execute. (Roadmap: a pluggable action shim registry.)
- Containers/services, reusable workflows (`uses:` at job level), concurrency,
  `permissions`, secrets injection (secrets resolve to empty).

Usage
-----
    gha.py run   WORKFLOW.yml --repo PATH --ref refs/heads/main [--sha SHA]
                 [--event push] [--job NAME] [--workspace DIR] [--keep]
    gha.py parse WORKFLOW.yml          # parse + print the normalized model
    gha.py jobs  WORKFLOW.yml --event push   # list jobs that would run

Exit code is 0 iff every executed job succeeded.
"""

import argparse
import itertools
import json
import os
import shutil
import subprocess
import sys
import tempfile

import yaml


# --------------------------------------------------------------------------
# Expression evaluator — a pragmatic subset of the GHA expression language.
# --------------------------------------------------------------------------

class ExprError(Exception):
    pass


class _Tok:
    def __init__(self, kind, value):
        self.kind = kind
        self.value = value

    def __repr__(self):
        return f"{self.kind}:{self.value!r}"


def _tokenize(s):
    toks = []
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c in " \t\r\n":
            i += 1
            continue
        if c in "'":  # string literal; '' is an escaped quote
            i += 1
            buf = []
            while i < n:
                if s[i] == "'" and i + 1 < n and s[i + 1] == "'":
                    buf.append("'")
                    i += 2
                    continue
                if s[i] == "'":
                    i += 1
                    break
                buf.append(s[i])
                i += 1
            toks.append(_Tok("str", "".join(buf)))
            continue
        if c.isdigit() or (c == "-" and i + 1 < n and s[i + 1].isdigit()):
            j = i + 1
            while j < n and (s[j].isdigit() or s[j] == "."):
                j += 1
            num = s[i:j]
            toks.append(_Tok("num", float(num) if "." in num else int(num)))
            i = j
            continue
        if c.isalpha() or c == "_":
            j = i
            while j < n and (s[j].isalnum() or s[j] in "_-"):
                j += 1
            word = s[i:j]
            low = word.lower()
            if low in ("true", "false", "null"):
                toks.append(_Tok("lit", {"true": True, "false": False, "null": None}[low]))
            else:
                toks.append(_Tok("name", word))
            i = j
            continue
        for op in ("==", "!=", ">=", "<=", "&&", "||"):
            if s.startswith(op, i):
                toks.append(_Tok("op", op))
                i += 2
                break
        else:
            if c in "().,![]<>*":
                toks.append(_Tok("op", c))
                i += 1
            else:
                raise ExprError(f"bad char {c!r} in expression {s!r}")
    toks.append(_Tok("end", None))
    return toks


class _Parser:
    """Recursive-descent parser → a small AST of nested tuples."""

    def __init__(self, toks):
        self.toks = toks
        self.pos = 0

    def peek(self):
        return self.toks[self.pos]

    def next(self):
        t = self.toks[self.pos]
        self.pos += 1
        return t

    def expect(self, kind, value=None):
        t = self.next()
        if t.kind != kind or (value is not None and t.value != value):
            raise ExprError(f"expected {kind} {value}, got {t}")
        return t

    def parse(self):
        node = self.parse_or()
        self.expect("end")
        return node

    def parse_or(self):
        node = self.parse_and()
        while self.peek().kind == "op" and self.peek().value == "||":
            self.next()
            node = ("or", node, self.parse_and())
        return node

    def parse_and(self):
        node = self.parse_cmp()
        while self.peek().kind == "op" and self.peek().value == "&&":
            self.next()
            node = ("and", node, self.parse_cmp())
        return node

    def parse_cmp(self):
        node = self.parse_unary()
        while self.peek().kind == "op" and self.peek().value in ("==", "!=", ">=", "<=", ">", "<"):
            op = self.next().value
            node = ("cmp", op, node, self.parse_unary())
        return node

    def parse_unary(self):
        if self.peek().kind == "op" and self.peek().value == "!":
            self.next()
            return ("not", self.parse_unary())
        return self.parse_postfix()

    def parse_postfix(self):
        node = self.parse_atom()
        while True:
            t = self.peek()
            if t.kind == "op" and t.value == ".":
                self.next()
                name = self.expect("name").value
                node = ("index", node, ("lit", name))
            elif t.kind == "op" and t.value == "[":
                self.next()
                idx = self.parse_or()
                self.expect("op", "]")
                node = ("index", node, idx)
            else:
                break
        return node

    def parse_atom(self):
        t = self.next()
        if t.kind == "str" or t.kind == "num" or t.kind == "lit":
            return ("lit", t.value)
        if t.kind == "op" and t.value == "(":
            node = self.parse_or()
            self.expect("op", ")")
            return node
        if t.kind == "op" and t.value == "*":
            return ("lit", "*")
        if t.kind == "name":
            if self.peek().kind == "op" and self.peek().value == "(":
                self.next()
                args = []
                if not (self.peek().kind == "op" and self.peek().value == ")"):
                    args.append(self.parse_or())
                    while self.peek().kind == "op" and self.peek().value == ",":
                        self.next()
                        args.append(self.parse_or())
                self.expect("op", ")")
                return ("call", t.value, args)
            return ("var", t.value)
        raise ExprError(f"unexpected token {t}")


def _truthy(v):
    return bool(v) and v != "false"


def _eval(node, ctx, status):
    kind = node[0]
    if kind == "lit":
        return node[1]
    if kind == "var":
        return ctx.get(node[1], "")
    if kind == "index":
        base = _eval(node[1], ctx, status)
        key = _eval(node[2], ctx, status)
        if isinstance(base, dict):
            return base.get(str(key), "")
        if isinstance(base, list):
            try:
                return base[int(key)]
            except (ValueError, IndexError, TypeError):
                return ""
        return ""
    if kind == "not":
        return not _truthy(_eval(node[1], ctx, status))
    if kind == "and":
        a = _eval(node[1], ctx, status)
        return _eval(node[2], ctx, status) if _truthy(a) else a
    if kind == "or":
        a = _eval(node[1], ctx, status)
        return a if _truthy(a) else _eval(node[2], ctx, status)
    if kind == "cmp":
        op, a, b = node[1], _eval(node[2], ctx, status), _eval(node[3], ctx, status)
        # GHA compares loosely; coerce to str for !=/== when types differ.
        if op == "==":
            return _loose_eq(a, b)
        if op == "!=":
            return not _loose_eq(a, b)
        try:
            if op == ">":
                return a > b
            if op == "<":
                return a < b
            if op == ">=":
                return a >= b
            if op == "<=":
                return a <= b
        except TypeError:
            return False
    if kind == "call":
        return _call(node[1], [_eval(a, ctx, status) for a in node[2]], status)
    raise ExprError(f"cannot eval {node}")


def _loose_eq(a, b):
    if type(a) == type(b):
        return a == b
    if isinstance(a, bool) or isinstance(b, bool):
        return _truthy(a) == _truthy(b)
    return str(a) == str(b)


def _call(name, args, status):
    low = name.lower()
    if low == "format":
        fmt = str(args[0])
        for idx, val in enumerate(args[1:]):
            fmt = fmt.replace("{" + str(idx) + "}", str(val))
        return fmt
    if low == "contains":
        hay, needle = args[0], args[1]
        if isinstance(hay, list):
            return needle in hay
        return str(needle) in str(hay)
    if low == "startswith":
        return str(args[0]).startswith(str(args[1]))
    if low == "endswith":
        return str(args[0]).endswith(str(args[1]))
    if low == "join":
        seq = args[0] if isinstance(args[0], list) else [args[0]]
        sep = str(args[1]) if len(args) > 1 else ","
        return sep.join(str(x) for x in seq)
    if low == "tojson":
        return json.dumps(args[0])
    if low == "fromjson":
        try:
            return json.loads(args[0])
        except (ValueError, TypeError):
            return ""
    if low == "hashfiles":
        return ""  # no content hashing in the MVP
    if low == "success":
        return status == "success"
    if low == "failure":
        return status == "failure"
    if low == "cancelled":
        return status == "cancelled"
    if low == "always":
        return True
    raise ExprError(f"unsupported function {name}()")


def eval_expr(expr, ctx, status="success"):
    """Evaluate a single GHA expression string (no surrounding ${{ }})."""
    return _eval(_Parser(_tokenize(expr)).parse(), ctx, status)


def eval_condition(value, ctx, status="success"):
    """Evaluate an `if:` condition. GHA accepts it bare (`success() && ...`) or
    wrapped (`${{ ... }}`); strip a single wrapper before evaluating. A bare
    non-expression string (e.g. `if: always`) is treated as truthy."""
    text = str(value).strip()
    if text.startswith("${{") and text.endswith("}}"):
        text = text[3:-2].strip()
    try:
        return _truthy(eval_expr(text, ctx, status))
    except ExprError as exc:
        sys.stderr.write(f"[gha] if-condition error in '{text}': {exc}\n")
        return False


def expand(template, ctx, status="success"):
    """Substitute every ${{ expr }} in `template` with its evaluated value."""
    if not isinstance(template, str):
        return template
    out = []
    i = 0
    while True:
        start = template.find("${{", i)
        if start < 0:
            out.append(template[i:])
            break
        out.append(template[i:start])
        end = template.find("}}", start)
        if end < 0:
            out.append(template[start:])
            break
        expr = template[start + 3:end].strip()
        try:
            val = eval_expr(expr, ctx, status)
        except ExprError as exc:
            val = ""
            sys.stderr.write(f"[gha] expression error in '{expr}': {exc}\n")
        out.append(str(val) if not isinstance(val, bool) else ("true" if val else "false"))
        i = end + 2
    return "".join(out)


# --------------------------------------------------------------------------
# Workflow model + parsing
# --------------------------------------------------------------------------

def load_workflow(path):
    with open(path) as handle:
        raw = yaml.safe_load(handle)
    if not isinstance(raw, dict):
        raise ValueError(f"{path}: not a YAML mapping")
    # The bare key `on:` is parsed by YAML 1.1 as the boolean True.
    triggers = raw.get("on", raw.get(True, {}))
    return {
        "name": raw.get("name", os.path.basename(path)),
        "on": _normalize_on(triggers),
        "env": raw.get("env", {}) or {},
        "jobs": raw.get("jobs", {}) or {},
    }


def _normalize_on(triggers):
    """on: may be a string, a list, or a mapping; normalize to {event: cfg}."""
    if triggers is None:
        return {}
    if isinstance(triggers, str):
        return {triggers: {}}
    if isinstance(triggers, list):
        return {ev: {} for ev in triggers}
    if isinstance(triggers, dict):
        return {str(k): (v or {}) for k, v in triggers.items()}
    return {}


def workflow_triggered(workflow, event):
    """Does `event` fire this workflow? (event-name match only; we don't filter
    on branch/tag/path globs in the MVP — they're advisory here.)"""
    return event in workflow["on"]


def matrix_combinations(strategy):
    """Expand strategy.matrix into a list of {var: value} dicts (cartesian
    product of the list-valued keys), applying include/exclude."""
    if not strategy or "matrix" not in strategy:
        return [{}]
    matrix = strategy["matrix"] or {}
    include = matrix.get("include", []) or []
    exclude = matrix.get("exclude", []) or []
    axes = {k: v for k, v in matrix.items() if k not in ("include", "exclude")}
    combos = []
    if axes:
        keys = list(axes.keys())
        for values in itertools.product(*[axes[k] if isinstance(axes[k], list) else [axes[k]] for k in keys]):
            combo = dict(zip(keys, values))
            if not any(_matches(combo, ex) for ex in exclude):
                combos.append(combo)
    # `include` adds combinations (or extends matching ones); MVP: append as-is.
    for inc in include:
        merged = False
        for combo in combos:
            if all(combo.get(k) == v for k, v in inc.items() if k in combo):
                combo.update(inc)
                merged = True
        if not merged:
            combos.append(dict(inc))
    return combos or [{}]


def _matches(combo, spec):
    return all(combo.get(k) == v for k, v in spec.items())


def topo_order(jobs):
    """Order job names so every job's `needs` come first. Raises on a cycle."""
    order, visiting, visited = [], set(), set()

    def visit(name):
        if name in visited:
            return
        if name in visiting:
            raise ValueError(f"needs cycle at job '{name}'")
        visiting.add(name)
        needs = jobs[name].get("needs", [])
        if isinstance(needs, str):
            needs = [needs]
        for dep in needs or []:
            if dep in jobs:
                visit(dep)
        visiting.discard(name)
        visited.add(name)
        order.append(name)

    for name in jobs:
        visit(name)
    return order


# --------------------------------------------------------------------------
# Execution
# --------------------------------------------------------------------------

class Logger:
    """Sink for execution output. Default writes to stderr; the runner-agent
    swaps in a callback that streams to the gateway's append_log."""

    def __init__(self, sink=None):
        self.sink = sink or (lambda line: sys.stdout.write(line))

    def line(self, text):
        if not text.endswith("\n"):
            text += "\n"
        self.sink(text)


def _github_context(event, ref, sha, repository):
    ref_name = ref.split("/", 2)[-1] if ref else ""
    return {
        "event_name": event,
        "ref": ref,
        "ref_name": ref_name,
        "sha": sha,
        "repository": repository,
        "workflow": "",
        "run_id": "0",
        "run_number": "1",
        "actor": "picoforge-runner",
        "event": {},
    }


def run_workflow(workflow, *, event, ref, sha, repo, repository,
                 only_job=None, workspace_root=None, keep=False, log=None,
                 runner_os="Linux"):
    """Execute the workflow. Returns 0 iff every executed job succeeded."""
    log = log or Logger()
    jobs = workflow["jobs"]
    if not jobs:
        log.line("[gha] no jobs in workflow")
        return 0

    cleanup_root = None
    if workspace_root is None:
        workspace_root = cleanup_root = tempfile.mkdtemp(prefix="gha-ws-")

    github = _github_context(event, ref, sha, repository)
    runner_ctx = {"os": runner_os, "arch": "X64", "temp": tempfile.gettempdir()}
    base_env = dict(workflow.get("env", {}))

    job_outputs = {}   # needs.<job>.outputs
    job_results = {}   # job -> "success"/"failure"/"skipped"
    overall_ok = True

    for job_name in topo_order(jobs):
        if only_job and job_name != only_job:
            continue
        job = jobs[job_name]

        # Skip a job whose dependency failed (GHA default: needs must succeed).
        needs = job.get("needs", [])
        if isinstance(needs, str):
            needs = [needs]
        dep_failed = any(job_results.get(d) not in (None, "success") for d in (needs or []))
        if dep_failed:
            log.line(f"[gha] job '{job_name}' SKIPPED (a dependency did not succeed)")
            job_results[job_name] = "skipped"
            continue

        for combo in matrix_combinations(job.get("strategy")):
            label = job_name + ("" if not combo else " " + json.dumps(combo, separators=(",", ":")))
            ctx = {
                "github": github,
                "matrix": combo,
                "runner": runner_ctx,
                "strategy": {"job-index": 0},
                "job": {"status": "success"},
                "needs": {n: {"outputs": job_outputs.get(n, {})} for n in (needs or [])},
                "env": dict(base_env, **(job.get("env", {}) or {})),
                "steps": {},
            }
            if "if" in job and not eval_condition(job["if"], ctx):
                log.line(f"[gha] job '{label}' SKIPPED (job-level if is false)")
                continue

            log.line(f"::group::job {label} (runs-on {job.get('runs-on', 'unspecified')})")
            ok = _run_job(job, ctx, github, sha, repo, workspace_root, job_name, combo, log)
            log.line("::endgroup::")
            status = "success" if ok else "failure"
            job_results[job_name] = status if job_results.get(job_name) != "failure" else "failure"
            if ok:
                # Resolve declared job outputs from the final step context.
                outs = {}
                for key, tmpl in (job.get("outputs", {}) or {}).items():
                    outs[key] = expand(str(tmpl), ctx)
                job_outputs[job_name] = outs
            else:
                overall_ok = False

    if cleanup_root and not keep:
        shutil.rmtree(cleanup_root, ignore_errors=True)
    elif cleanup_root:
        log.line(f"[gha] workspace kept at {cleanup_root}")
    return 0 if overall_ok else 1


def _run_job(job, ctx, github, sha, repo, workspace_root, job_name, combo, log):
    """Run one job-run (a single matrix combination). Returns True on success."""
    safe = "".join(c if c.isalnum() else "-" for c in (job_name + "-" + "-".join(map(str, combo.values()))))
    workspace = os.path.join(workspace_root, "ws-" + (safe or job_name))
    # GITHUB_WORKSPACE holds the checked-out repo; the $GITHUB_OUTPUT/ENV/PATH
    # files live OUTSIDE it (real GHA keeps them in RUNNER_TEMP) so the workspace
    # stays empty for `actions/checkout`'s clone.
    meta = os.path.join(workspace_root, "meta-" + (safe or job_name))
    os.makedirs(workspace, exist_ok=True)
    os.makedirs(meta, exist_ok=True)
    env_file = os.path.join(meta, "env")
    out_file = os.path.join(meta, "out")
    path_file = os.path.join(meta, "path")
    summary_file = os.path.join(meta, "summary")
    job_env = dict(ctx["env"])
    extra_path = []
    status = "success"

    steps = job.get("steps", []) or []
    for index, step in enumerate(steps):
        step_id = step.get("id", f"__step{index}")
        name = expand(str(step.get("name", step.get("uses") or step.get("run", "step")[:40])), ctx)
        if "if" in step:
            if not eval_condition(step["if"], ctx, status):
                log.line(f"[gha] step '{name}' SKIPPED (if false)")
                continue
        elif status == "failure":
            # Default: once a step fails the rest are skipped (no continue-on-error).
            log.line(f"[gha] step '{name}' SKIPPED (a previous step failed)")
            continue

        # Fresh per-step output/env/path files.
        for f in (out_file, env_file, path_file):
            open(f, "w").close()

        step_env = dict(job_env)
        step_env.update({expand(str(k), ctx): expand(str(v), ctx) for k, v in (step.get("env", {}) or {}).items()})
        step_env.update(_github_env(github, sha, workspace, env_file, out_file, path_file, summary_file,
                                    ctx["runner"]["os"]))
        if extra_path:
            step_env["PATH"] = os.pathsep.join(extra_path) + os.pathsep + step_env.get("PATH", os.environ.get("PATH", ""))

        uses = step.get("uses")
        ok = True
        if uses:
            ok = _run_uses(uses, step, ctx, repo, sha, github, workspace, log)
        elif "run" in step:
            ok = _run_script(step, ctx, status, workspace, step_env, log, name)
        else:
            log.line(f"[gha] step '{name}': nothing to do (no run/uses)")

        # Absorb $GITHUB_OUTPUT → steps.<id>.outputs, $GITHUB_ENV → job env,
        # $GITHUB_PATH → PATH for later steps.
        outputs = _read_kv_file(out_file)
        ctx["steps"][step_id] = {"outputs": outputs, "outcome": "success" if ok else "failure",
                                 "conclusion": "success" if ok else "failure"}
        job_env.update(_read_kv_file(env_file))
        ctx["env"] = dict(ctx["env"], **_read_kv_file(env_file))
        if os.path.exists(path_file):
            with open(path_file) as handle:
                extra_path.extend(line.strip() for line in handle if line.strip())
        if not ok:
            status = "failure"
            ctx["job"]["status"] = "failure"

    return status == "success"


def _github_env(github, sha, workspace, env_file, out_file, path_file, summary_file, runner_os):
    return {
        "CI": "true",
        "GITHUB_ACTIONS": "true",
        "GITHUB_WORKSPACE": workspace,
        "GITHUB_SHA": sha or "",
        "GITHUB_REF": github["ref"] or "",
        "GITHUB_REF_NAME": github["ref_name"] or "",
        "GITHUB_EVENT_NAME": github["event_name"] or "",
        "GITHUB_REPOSITORY": github["repository"] or "",
        "GITHUB_OUTPUT": out_file,
        "GITHUB_ENV": env_file,
        "GITHUB_PATH": path_file,
        "GITHUB_STEP_SUMMARY": summary_file,
        "RUNNER_OS": runner_os,
        "RUNNER_TEMP": tempfile.gettempdir(),
    }


def _run_uses(uses, step, ctx, repo, sha, github, workspace, log):
    action = uses.split("@", 1)[0]
    if action in ("actions/checkout", "./.github/actions/checkout"):
        return _checkout(repo, sha, github, workspace, step, ctx, log)
    log.line(f"[gha] step uses '{uses}' — UNSUPPORTED marketplace action, SKIPPED "
             f"(only actions/checkout runs in this engine; run: steps still execute)")
    return True  # skip, do not fail the job on an unsupported action


def _checkout(repo, sha, github, workspace, step, ctx, log):
    """actions/checkout: clone `repo` into the workspace at the requested ref."""
    if not repo:
        log.line("[gha] checkout: no repo source provided (--repo) — skipping clone")
        return True
    target = workspace
    with_ = step.get("with", {}) or {}
    want_ref = expand(str(with_.get("ref", sha or github["ref"] or "HEAD")), ctx)
    log.line(f"[gha] checkout {repo} @ {want_ref or 'HEAD'} -> {target}")
    try:
        # Clone from the (local bare or working) repo; for the dev/local mesh the
        # descriptor hands the runner the bare repo path. A clone keeps the
        # workspace isolated from the source.
        if os.path.exists(os.path.join(target, ".git")):
            subprocess.run(["git", "-C", target, "fetch", "--all", "--tags"], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        else:
            subprocess.run(["git", "clone", "--no-single-branch", repo, target], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if want_ref and want_ref != "HEAD":
            subprocess.run(["git", "-C", target, "checkout", "--force", want_ref], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return True
    except (subprocess.CalledProcessError, OSError) as exc:
        out = getattr(exc, "output", b"")
        if isinstance(out, bytes):
            out = out.decode("utf-8", "replace")
        log.line(f"[gha] checkout FAILED: {exc}\n{out}")
        return False


def _run_script(step, ctx, status, workspace, env, log, name):
    script = expand(str(step["run"]), ctx, status)
    shell = step.get("shell", "bash")
    workdir = expand(str(step.get("working-directory", workspace)), ctx)
    if not os.path.isabs(workdir):
        workdir = os.path.join(workspace, workdir)
    os.makedirs(workdir, exist_ok=True)
    log.line(f"[gha] run '{name}' (shell={shell}, cwd={os.path.relpath(workdir, workspace) or '.'})")
    if shell in ("bash", "sh"):
        # GHA bash default flags: -e (and pipefail via the wrapper).
        argv = ["bash", "-eo", "pipefail", "-c", script] if shell == "bash" else ["sh", "-e", "-c", script]
    elif shell == "python":
        argv = ["python3", "-c", script]
    else:
        argv = [shell, "-c", script]
    proc = subprocess.run(argv, cwd=workdir, env={**os.environ, **env},
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if proc.stdout:
        for line in proc.stdout.splitlines():
            log.line(line)
    if proc.returncode != 0:
        log.line(f"[gha] step '{name}' FAILED (exit {proc.returncode})")
        return False
    return True


def _read_kv_file(path):
    """Parse a $GITHUB_OUTPUT/$GITHUB_ENV file: `name=value` and the heredoc
    form `name<<DELIM\\n...\\nDELIM`."""
    result = {}
    if not os.path.exists(path):
        return result
    with open(path) as handle:
        lines = handle.read().splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if "<<" in line:
            key, delim = line.split("<<", 1)
            key, delim = key.strip(), delim.strip()
            buf = []
            i += 1
            while i < len(lines) and lines[i] != delim:
                buf.append(lines[i])
                i += 1
            result[key] = "\n".join(buf)
        elif "=" in line:
            key, val = line.split("=", 1)
            result[key.strip()] = val
        i += 1
    return result


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Picoforge GitHub Actions workflow engine")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_run = sub.add_parser("run", help="execute a workflow")
    p_run.add_argument("workflow")
    p_run.add_argument("--repo", default=None, help="repo source to checkout (path or URL)")
    p_run.add_argument("--ref", default="refs/heads/main")
    p_run.add_argument("--sha", default="")
    p_run.add_argument("--event", default="workflow_dispatch")
    p_run.add_argument("--repository", default="picoforge/repo")
    p_run.add_argument("--job", default=None, help="run only this job")
    p_run.add_argument("--workspace", default=None, help="workspace root (default: temp)")
    p_run.add_argument("--keep", action="store_true", help="keep the workspace")

    p_parse = sub.add_parser("parse", help="parse + print the normalized model")
    p_parse.add_argument("workflow")

    p_jobs = sub.add_parser("jobs", help="list jobs that would run for an event")
    p_jobs.add_argument("workflow")
    p_jobs.add_argument("--event", default="push")

    args = parser.parse_args()
    workflow = load_workflow(args.workflow)

    if args.cmd == "parse":
        json.dump({"name": workflow["name"], "on": list(workflow["on"].keys()),
                   "jobs": {n: {"needs": j.get("needs", []),
                                "runs-on": j.get("runs-on"),
                                "steps": len(j.get("steps", []) or []),
                                "matrix": list((j.get("strategy", {}) or {}).get("matrix", {}).keys())}
                            for n, j in workflow["jobs"].items()}},
                  sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    if args.cmd == "jobs":
        fires = workflow_triggered(workflow, args.event)
        print(f"workflow '{workflow['name']}' triggered by '{args.event}': {fires}")
        for name in topo_order(workflow["jobs"]):
            job = workflow["jobs"][name]
            combos = matrix_combinations(job.get("strategy"))
            print(f"  {name}: {len(combos)} run(s), needs={job.get('needs', [])}")
        return 0

    if args.cmd == "run":
        if not workflow_triggered(workflow, args.event):
            sys.stderr.write(f"[gha] workflow not triggered by '{args.event}' "
                             f"(on: {list(workflow['on'].keys())}) — running anyway\n")
        return run_workflow(workflow, event=args.event, ref=args.ref, sha=args.sha,
                            repo=args.repo, repository=args.repository,
                            only_job=args.job, workspace_root=args.workspace, keep=args.keep)
    return 2


if __name__ == "__main__":
    sys.exit(main())
