#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyyaml"]
# ///
"""Picoforge runner agent (docs/runner-agent.md, MVP).

An external process that authenticates to the gateway with an opaque
`rnr_<secret>` bearer token, registers itself, then poll-leases pipeline jobs,
executes them locally, streams logs back, and reports completion. The gateway
never connects to the agent — the agent is always the client, so this works
through NAT/firewalls with no inbound listener.

The runner token + runner id are handed out once by an admin via
`runner_agent.runner_agent.create_token`. Pass them here:

    runner-agent.py --runner-id 1 --token rnr_xx…           \\
                    --gateway http://127.0.0.1:8090 --labels linux,x86_64

Two lease cadences are supported (pick with --lease-wait):

- Long polling (default, GitLab/GitHub-runner style): the lease call asks the
  gateway to HOLD the request open until a job is ready or a wait window
  elapses. The agent re-issues immediately on an empty return — no client-side
  idle sleep — so job pickup is near-instant with one in-flight request.
- Classic short polling (--lease-wait 0): 1s + jitter while idle, backing off
  to 2/5/15s after repeated empty leases, snapping back on any activity.

Heartbeats go out on a slower ~15s cadence either way.

Transport is `curl` (not urllib): the connection layer is deliberately isolated
from the RPC envelope and from job execution, so pointing the agent at an
`https://` gateway later — or adding mTLS / a proxy / a custom CA — is a matter
of curl flags, with nothing else to change.

Use --once to lease/execute/report exactly one job and exit (smoke testing).
"""

import argparse
import json
import os
import random
import signal
import socket
import subprocess
import sys
import tempfile
import time

# The GitHub Actions workflow engine lives beside this script (issue #33).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import gha
except ImportError:
    gha = None


class RpcError(Exception):
    """A gateway call failed (transport, auth, or backend error)."""


class TransportError(Exception):
    """The HTTP transport (curl) failed before a complete response arrived."""


class CurlTransport:
    """HTTP transport backed by the `curl` binary.

    This is the ONE place that knows how bytes reach the gateway. Everything
    above it (the RPC envelope in `Gateway`, the lease loop, job execution) is
    transport-agnostic. Routing through curl rather than urllib means an
    `https://` gateway needs no code change here — curl handles TLS — and
    future transport knobs (`--cacert`, `--cert`, `-x <proxy>`, HTTP/2) are
    plain curl flags passed via `extra_args`."""

    def __init__(self, curl_path="curl", extra_args=None):
        self.curl_path = curl_path
        self.extra_args = list(extra_args or [])

    def post(self, url, headers, body, timeout):
        """POST `body` (bytes) to `url`. Returns (status_code, response_bytes).

        `timeout` is the whole-request budget in seconds, handed to curl as
        --max-time; long-poll leases pass a budget that outlasts the gateway's
        own hold so the held connection is never cut client-side."""
        response_fd, response_path = tempfile.mkstemp(prefix="runner-resp-")
        os.close(response_fd)
        command = [
            self.curl_path,
            "--silent", "--show-error",
            "--max-time", f"{timeout:.1f}",
            "--output", response_path,      # response body → file
            "--write-out", "%{http_code}",  # status code → stdout
            "--request", "POST",
            "--data-binary", "@-",          # request body ← stdin
        ]
        for key, value in headers.items():
            command += ["--header", f"{key}: {value}"]
        command += self.extra_args
        command.append(url)
        try:
            completed = subprocess.run(
                command, input=body, capture_output=True, timeout=timeout + 10
            )
        except FileNotFoundError:
            os.unlink(response_path)
            raise TransportError(f"curl binary not found: {self.curl_path!r}") from None
        except subprocess.TimeoutExpired:
            os.unlink(response_path)
            raise TransportError(f"curl wall-clock timed out after {timeout:.1f}s") from None
        try:
            if completed.returncode != 0:
                detail = completed.stderr.decode("utf-8", "replace").strip()
                raise TransportError(
                    f"curl exit {completed.returncode}: {detail or '(no stderr)'}")
            status_text = completed.stdout.decode("ascii", "replace").strip()
            if not status_text.isdigit():
                raise TransportError(f"curl emitted no status code (got {status_text!r})")
            with open(response_path, "rb") as handle:
                return int(status_text), handle.read()
        finally:
            try:
                os.unlink(response_path)
            except OSError:
                pass


class Gateway:
    """JSON `/_rpc` client carrying the runner's opaque bearer token.

    Owns only the RPC envelope (path + args → result); the wire is delegated to
    a transport so the connection logic stays separate from everything else."""

    def __init__(self, base_url, token, transport=None, http_timeout=15.0):
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.transport = transport or CurlTransport()
        self.http_timeout = http_timeout

    def call(self, path, args, timeout=None):
        body = json.dumps({"path": path, "args": args}).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
            "Authorization": "Bearer " + self.token,
        }
        try:
            status, payload = self.transport.post(
                self.base_url + "/_rpc", headers, body,
                self.http_timeout if timeout is None else timeout,
            )
        except TransportError as error:
            raise RpcError(f"{path}: {error}") from None
        text = payload.decode("utf-8", "replace")
        if status != 200:
            raise RpcError(f"{path}: HTTP {status}: {text[:300]}")
        try:
            parsed = json.loads(text)
        except json.JSONDecodeError as error:
            raise RpcError(f"{path}: malformed response: {error}") from None
        if not isinstance(parsed, dict) or "result" not in parsed:
            raise RpcError(f"{path}: malformed response: {parsed!r}")
        return parsed["result"]


def build_command(descriptor):
    """The shell command a leased job runs.

    MVP: if the job's `pipeline_path` names an existing local file, run it with
    /bin/sh; otherwise run a built-in stub that echoes the job metadata. Either
    way the runner demonstrates lease → execute → log → complete end to end.
    """
    job_id = descriptor.get("job_id")
    repo_id = descriptor.get("repo_id")
    ref = descriptor.get("ref") or "(none)"
    pipeline_path = descriptor.get("pipeline_path") or ""
    if pipeline_path and os.path.isfile(pipeline_path):
        return ["/bin/sh", pipeline_path]
    stub = (
        f'echo "[runner] job {job_id} repo {repo_id} ref {ref}";'
        f'echo "[runner] no pipeline file ({pipeline_path or "unset"}) — running stub build";'
        'echo "[runner] step 1/2: prepare"; '
        'echo "[runner] step 2/2: build OK"'
    )
    return ["/bin/sh", "-c", stub]


def _descriptor_is_workflow(descriptor):
    """A leased job is a GitHub Actions workflow run when the mesh hands us the
    workflow YAML (`workflow_content`) or its path points at .github/workflows."""
    if descriptor.get("workflow_content"):
        return True
    path = descriptor.get("pipeline_path") or ""
    return path.endswith((".yml", ".yaml")) and "workflows" in path


def execute_workflow(gateway, descriptor):
    """Run a GitHub Actions workflow via the bundled engine (issue #33),
    streaming every line to append_log. The mesh descriptor provides the
    workflow YAML and the repo source; missing pieces degrade gracefully.
    Returns (status, summary)."""
    job_id = descriptor["job_id"]
    if gha is None:
        gateway.call("git_pipeline.git_pipeline.append_log",
                     [job_id, 0, "[runner] GHA engine unavailable (pyyaml missing)\n"])
        return 1, "gha engine unavailable"

    offset = [0]

    def sink(line):
        if not line.endswith("\n"):
            line += "\n"
        gateway.call("git_pipeline.git_pipeline.append_log", [job_id, offset[0], line])
        offset[0] += len(line.encode("utf-8"))

    # Obtain the workflow definition: inline content from the descriptor, or a
    # local path (dev/local mesh where the runner shares the repo's disk).
    content = descriptor.get("workflow_content")
    wf_path = descriptor.get("pipeline_path") or ""
    tmp_wf = None
    try:
        if content:
            tmp_wf = tempfile.NamedTemporaryFile("w", suffix=".yml", delete=False)
            tmp_wf.write(content)
            tmp_wf.close()
            wf_path = tmp_wf.name
        if not wf_path or not os.path.isfile(wf_path):
            sink(f"[runner] workflow file not available ({wf_path or 'unset'})")
            return 1, "workflow unavailable"
        workflow = gha.load_workflow(wf_path)
        rc = gha.run_workflow(
            workflow,
            event=descriptor.get("event") or "workflow_dispatch",
            ref=descriptor.get("ref") or "HEAD",
            sha=descriptor.get("sha") or "",
            repo=descriptor.get("repo_url") or descriptor.get("repo_path"),
            repository=descriptor.get("repository") or "picoforge/repo",
            only_job=descriptor.get("job") or None,
            log=gha.Logger(sink),
        )
        return (0, "workflow succeeded") if rc == 0 else (1, "workflow failed")
    except Exception as error:  # engine errors must not crash the runner
        sink(f"[runner] workflow engine error: {error}")
        return 1, "workflow engine error"
    finally:
        if tmp_wf is not None:
            try:
                os.unlink(tmp_wf.name)
            except OSError:
                pass


def execute_job(gateway, descriptor):
    """Run the job locally, streaming stdout to append_log. Returns (status,
    summary) where status is 0 on success and 1 on failure."""
    if _descriptor_is_workflow(descriptor):
        return execute_workflow(gateway, descriptor)
    job_id = descriptor["job_id"]
    timeout = float(descriptor.get("timeout") or 300)
    command = build_command(descriptor)
    offset = 0
    started = time.monotonic()
    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except OSError as error:
        message = f"[runner] failed to start job: {error}\n"
        gateway.call("git_pipeline.git_pipeline.append_log", [job_id, offset, message])
        return 1, "spawn failed"

    assert process.stdout is not None
    for line in process.stdout:
        gateway.call("git_pipeline.git_pipeline.append_log", [job_id, offset, line])
        offset += len(line.encode("utf-8"))
        if time.monotonic() - started > timeout:
            process.kill()
            gateway.call(
                "git_pipeline.git_pipeline.append_log",
                [job_id, offset, "[runner] timed out — killed\n"],
            )
            return 1, "timed out"
    exit_code = process.wait()
    status = 0 if exit_code == 0 else 1
    summary = "succeeded" if status == 0 else f"failed (exit {exit_code})"
    return status, summary


def lease_next(gateway, runner_id, labels, wait_ms, http_timeout):
    """Ask the gateway for the next matching job (`wait_ms` > 0 = long-poll).

    Returns the job descriptor or a falsy value when the queue is empty. The
    `wait_ms` rides through to the gateway and on to git_pipeline, which holds
    the call open for up to that window; the HTTP budget must outlast it."""
    return gateway.call(
        "git_pipeline.git_pipeline.lease_job",
        [runner_id, labels, wait_ms],
        timeout=http_timeout,
    )


def run(arguments):
    transport = CurlTransport(curl_path=arguments.curl)
    gateway = Gateway(arguments.gateway, arguments.token, transport=transport,
                      http_timeout=arguments.http_timeout)
    runner_id = arguments.runner_id
    host_info = arguments.host or socket.gethostname()

    registered = gateway.call(
        "runner_agent.runner_agent.register",
        [runner_id, arguments.name, arguments.labels, arguments.version, host_info],
    )

    # Long-poll wait in milliseconds; 0 selects classic short polling. The HTTP
    # budget for a held lease must outlast the server's own hold, hence the pad.
    long_poll_ms = int(max(0.0, arguments.lease_wait) * 1000)
    lease_http_timeout = (
        arguments.lease_wait + 15.0 if long_poll_ms > 0 else arguments.http_timeout
    )
    mode = f"long-poll {arguments.lease_wait:g}s" if long_poll_ms > 0 else "short-poll"
    print(f"[runner] registered runner_id={registered} labels={arguments.labels} "
          f"lease={mode}", flush=True)

    backoff_steps = [arguments.poll, 2.0, 5.0, 15.0]
    empty_polls = 0
    last_heartbeat = 0.0

    while True:
        now = time.monotonic()
        if now - last_heartbeat >= arguments.heartbeat:
            try:
                gateway.call("runner_agent.runner_agent.heartbeat", [runner_id, "online"])
            except RpcError as error:
                print(f"[runner] heartbeat failed: {error}", file=sys.stderr, flush=True)
            last_heartbeat = now

        try:
            descriptor = lease_next(
                gateway, runner_id, arguments.labels, long_poll_ms, lease_http_timeout
            )
        except RpcError as error:
            print(f"[runner] lease failed: {error}", file=sys.stderr, flush=True)
            time.sleep(arguments.poll)
            continue

        if not descriptor:
            if arguments.once:
                print("[runner] --once: no job available, exiting", flush=True)
                return 0
            if long_poll_ms > 0:
                # The gateway already held the connection for the wait window —
                # re-issue at once; no client-side idle sleep.
                continue
            empty_polls += 1
            step = backoff_steps[min(empty_polls, len(backoff_steps) - 1)]
            jitter = step * random.uniform(-0.2, 0.2)
            time.sleep(max(0.1, step + jitter))
            continue

        empty_polls = 0
        job_id = descriptor["job_id"]
        print(f"[runner] leased job {job_id} (ref={descriptor.get('ref')})", flush=True)
        status, summary = execute_job(gateway, descriptor)
        gateway.call("git_pipeline.git_pipeline.complete_job", [job_id, status, summary])
        print(f"[runner] completed job {job_id}: {summary}", flush=True)
        if arguments.once:
            return 0


def main():
    parser = argparse.ArgumentParser(description="Picoforge runner agent")
    parser.add_argument("--gateway", default=os.environ.get("PICOFORGE_GATEWAY", "http://127.0.0.1:8090"),
                        help="gateway base URL (default http://127.0.0.1:8090)")
    parser.add_argument("--token", default=os.environ.get("PICOFORGE_RUNNER_TOKEN"),
                        help="opaque rnr_ runner token (or $PICOFORGE_RUNNER_TOKEN)")
    parser.add_argument("--runner-id", type=int, default=os.environ.get("PICOFORGE_RUNNER_ID"),
                        help="runner id from create_token (or $PICOFORGE_RUNNER_ID)")
    parser.add_argument("--name", default=socket.gethostname(), help="display name")
    parser.add_argument("--labels", default="linux,x86_64", help="comma-separated capabilities")
    parser.add_argument("--version", default="0.1.0", help="runner agent version")
    parser.add_argument("--host", default=None, help="host info (default: hostname)")
    parser.add_argument("--poll", type=float, default=1.0,
                        help="short-poll idle seconds, used only when --lease-wait 0 (default 1.0)")
    parser.add_argument("--lease-wait", type=float,
                        default=float(os.environ.get("PICOFORGE_LEASE_WAIT", "25")),
                        help="long-poll hold seconds the gateway may keep a lease open; "
                             "0 disables long polling and falls back to short polling (default 25)")
    parser.add_argument("--http-timeout", type=float, default=15.0,
                        help="per-request curl budget for non-lease calls (default 15.0)")
    parser.add_argument("--curl", default=os.environ.get("PICOFORGE_CURL", "curl"),
                        help="curl binary to use as the HTTP transport (default 'curl')")
    parser.add_argument("--heartbeat", type=float, default=15.0, help="heartbeat interval seconds")
    parser.add_argument("--once", action="store_true", help="lease/run one job then exit")
    arguments = parser.parse_args()

    if not arguments.token:
        parser.error("a runner token is required (--token or $PICOFORGE_RUNNER_TOKEN)")
    if arguments.runner_id is None:
        parser.error("a runner id is required (--runner-id or $PICOFORGE_RUNNER_ID)")
    arguments.runner_id = int(arguments.runner_id)

    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    try:
        return run(arguments)
    except KeyboardInterrupt:
        return 0
    except RpcError as error:
        print(f"[runner] fatal: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
