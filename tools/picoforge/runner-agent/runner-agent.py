#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = []
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

Use --once to lease/execute/report exactly one job and exit (smoke testing).

Polling cadence (per the doc): 1s + jitter while idle, backing off to 2/5/15s
after repeated empty leases, snapping back to 1s on any activity. Heartbeats go
out on a slower ~15s cadence.
"""

import argparse
import json
import os
import random
import signal
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request


class RpcError(Exception):
    """A gateway call failed (transport, auth, or backend error)."""


class Gateway:
    """Thin JSON `/_rpc` client carrying the runner's opaque bearer token."""

    def __init__(self, base_url, token, http_timeout=15.0):
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.http_timeout = http_timeout

    def call(self, path, args):
        body = json.dumps({"path": path, "args": args}).encode("utf-8")
        request = urllib.request.Request(self.base_url + "/_rpc", data=body, method="POST")
        request.add_header("Content-Type", "application/json")
        request.add_header("Authorization", "Bearer " + self.token)
        try:
            with urllib.request.urlopen(request, timeout=self.http_timeout) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")[:300]
            raise RpcError(f"{path}: HTTP {error.code}: {detail}") from None
        except (urllib.error.URLError, OSError) as error:
            raise RpcError(f"{path}: {error}") from None
        if not isinstance(payload, dict) or "result" not in payload:
            raise RpcError(f"{path}: malformed response: {payload!r}")
        return payload["result"]


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


def execute_job(gateway, descriptor):
    """Run the job locally, streaming stdout to append_log. Returns (status,
    summary) where status is 0 on success and 1 on failure."""
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


def run(arguments):
    gateway = Gateway(arguments.gateway, arguments.token)
    runner_id = arguments.runner_id
    host_info = arguments.host or socket.gethostname()

    registered = gateway.call(
        "runner_agent.runner_agent.register",
        [runner_id, arguments.name, arguments.labels, arguments.version, host_info],
    )
    print(f"[runner] registered runner_id={registered} labels={arguments.labels}", flush=True)

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
            descriptor = gateway.call(
                "git_pipeline.git_pipeline.lease_job", [runner_id, arguments.labels]
            )
        except RpcError as error:
            print(f"[runner] lease failed: {error}", file=sys.stderr, flush=True)
            time.sleep(arguments.poll)
            continue

        if not descriptor:
            empty_polls += 1
            step = backoff_steps[min(empty_polls, len(backoff_steps) - 1)]
            jitter = step * random.uniform(-0.2, 0.2)
            time.sleep(max(0.1, step + jitter))
            if arguments.once:
                print("[runner] --once: no job available, exiting", flush=True)
                return 0
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
    parser.add_argument("--poll", type=float, default=1.0, help="idle poll seconds (default 1.0)")
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
