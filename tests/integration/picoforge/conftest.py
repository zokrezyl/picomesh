"""Pytest fixtures for the picoforge integration tests.

Three things are provisioned:

  * `base_url` — a live picoforge webapp to drive. By default the fixture
    brings the WHOLE stack up itself (control parent -> reconcile spawns the
    gateway + backends -> picoforge-webapp) against a fresh, wiped
    /tmp/picoforge, and tears it all down afterwards. Set
    PICOFORGE_WEBAPP_URL=http://host:port to instead drive an already-running
    stack and skip lifecycle management.

  * `gateway_url` — the gateway's API endpoint (:8090 by default). The
    mesh-correctness tests exercise the reflected RPC surface (/_describe,
    /_rpc) here; per the project invariant they go through the gateway, never
    a backend port.

  * `page` — a Playwright page backed by the system Google Chrome
    (channel="chrome"), so no `playwright install` browser download is needed.

Lifecycle is the mesh-sanctioned one: the control parent owns the backend
children it spawned, so we SIGTERM the parent (mesh-down.sh / Popen.terminate)
and let its reaper take the children down. We never pkill, never pattern-kill,
and never signal a backend port directly. The picoforge-webapp is a standalone
sidecar this harness launches and owns, so it is torn down via its own Popen
handle. To avoid fighting a leftover sidecar for the default webapp port, the
harness auto-picks a free port (the webapp port is arbitrary — it is only a
gateway client).
"""

import json
import os
import socket
import subprocess
import time
import urllib.request

import pytest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
PICOMESH = os.path.join(REPO_ROOT, "build-desktop-release", "picomesh")
WEBAPP = os.path.join(REPO_ROOT, "build-desktop-release", "picoforge-webapp")
CONFIG = os.path.join(REPO_ROOT, "assets", "picoforge", "config", "picoforge.yaml")
STATIC = os.path.join(REPO_ROOT, "assets", "picoforge", "static")
MESH_DOWN = os.path.join(REPO_ROOT, "tools", "picoforge", "mesh-down.sh")
MESH_DB = "/tmp/picoforge/mesh-state.db"

# Control parent + gateway ports are fixed by the config; the webapp port is a
# preferred default the harness will fall back off of if it is busy. Override
# the parent/gateway via env only if the config is also changed to match.
CTRL = int(os.environ.get("PICOFORGE_CTRL_PORT", "8800"))
WEB = int(os.environ.get("PICOFORGE_GATEWAY_PORT", "8090"))
SIDE_PREFERRED = int(os.environ.get("PICOFORGE_WEBAPP_PORT", "8080"))


def _port_open(port, host="127.0.0.1"):
    with socket.socket() as sock:
        sock.settimeout(0.3)
        try:
            sock.connect((host, port))
            return True
        except OSError:
            return False


def _wait_port(port, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if _port_open(port):
            return True
        time.sleep(0.25)
    return False


def _wait_port_free(port, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if not _port_open(port):
            return True
        time.sleep(0.2)
    return False


def _free_port(preferred):
    """`preferred` if bindable right now, else an OS-assigned free port. The
    webapp is only a gateway client, so its port is arbitrary — auto-picking
    means a leftover sidecar holding the default port never blocks a run, and
    we never have to kill a foreign process to reclaim it."""
    with socket.socket() as sock:
        try:
            sock.bind(("127.0.0.1", preferred))
            return preferred
        except OSError:
            pass
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def _post_json(url, obj):
    data = json.dumps(obj).encode()
    req = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read())


def _bring_stale_mesh_down():
    """Stop a stack left behind by a prior aborted run — the mesh way, never
    pkill. mesh-down.sh reads the control parent's pid from mesh storage and
    SIGTERMs it; the parent's reaper takes the backend children it spawned
    down. We do not pattern-kill and we do not touch a backend port."""
    if os.path.exists(MESH_DB):
        subprocess.run(["bash", MESH_DOWN, MESH_DB],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        _wait_port_free(WEB, 8)
        _wait_port_free(CTRL, 8)


@pytest.fixture(scope="session")
def base_url():
    external = os.environ.get("PICOFORGE_WEBAPP_URL")
    if external:
        url = external.rstrip("/")
        if not _wait_port(int(url.rsplit(":", 1)[1]), 5):
            pytest.skip(f"PICOFORGE_WEBAPP_URL={url} is not reachable")
        yield url
        return

    for binary in (PICOMESH, WEBAPP):
        if not os.path.exists(binary):
            pytest.skip(f"{binary} missing — run `make build-desktop-release` first")

    _bring_stale_mesh_down()
    subprocess.run(["rm", "-rf", "/tmp/picoforge"])
    os.makedirs("/tmp/picoforge", exist_ok=True)

    if not _wait_port_free(CTRL, 5):
        pytest.skip(f"control port {CTRL} is busy — stop the stack holding it")

    parent = subprocess.Popen(
        [PICOMESH, "--config-file", CONFIG, "--frontend", "yhttp",
         "--host", "127.0.0.1", "--port", str(CTRL), "serve"],
        stdout=open("/tmp/picoforge/itest-parent.log", "w"),
        stderr=subprocess.STDOUT)
    webapp = None
    side = _free_port(SIDE_PREFERRED)
    try:
        assert _wait_port(CTRL, 15), "control parent did not bind"
        handle = _post_json(f"http://127.0.0.1:{CTRL}/create",
                            {"class": "mesh_mesh"})["handle"]
        _post_json(f"http://127.0.0.1:{CTRL}/invoke",
                   {"method": "mesh_mesh_reconcile_from_config",
                    "handle": handle, "args": []})
        assert _wait_port(WEB, 30), "gateway did not bind"
        time.sleep(1.5)  # let backends finish binding before first call

        webapp = subprocess.Popen(
            [WEBAPP, "--gateway-url", f"http://127.0.0.1:{WEB}",
             "--host", "127.0.0.1", "--port", str(side), "--static", STATIC],
            stdout=open("/tmp/picoforge/itest-webapp.log", "w"),
            stderr=subprocess.STDOUT)
        assert _wait_port(side, 15), "webapp did not bind"

        yield f"http://127.0.0.1:{side}"
    finally:
        # Sanctioned teardown: SIGTERM the sidecar we own, then the control
        # parent — whose reaper stops the backend children it spawned. No
        # pkill, no signalling a backend port directly.
        if webapp:
            webapp.terminate()
        parent.terminate()
        _wait_port_free(side, 8)
        _wait_port_free(WEB, 8)
        _wait_port_free(CTRL, 8)


@pytest.fixture(scope="session")
def gateway_url(base_url):
    """The gateway's API endpoint. Tests that exercise the RPC surface
    (/_describe, /_rpc) target this — never a backend port. Derived from the
    webapp host + the gateway port, or PICOFORGE_GATEWAY_URL when driving an
    external stack whose gateway is elsewhere."""
    explicit = os.environ.get("PICOFORGE_GATEWAY_URL")
    if explicit:
        return explicit.rstrip("/")
    host = base_url.rsplit(":", 1)[0]
    return f"{host}:{WEB}"


@pytest.fixture(scope="session")
def _browser():
    sync_playwright = pytest.importorskip(
        "playwright.sync_api",
        reason="playwright not installed (run via run.sh / `uv run --with playwright`)",
    ).sync_playwright
    try:
        pw = sync_playwright().start()
        browser = pw.chromium.launch(channel="chrome", headless=True,
                                     args=["--no-sandbox"])
    except Exception as e:  # noqa: BLE001 — surface as a skip, not a hard error
        pytest.skip(f"cannot launch system Chrome via Playwright: {e}")
    yield browser
    browser.close()
    pw.stop()


@pytest.fixture
def page(_browser):
    ctx = _browser.new_context()
    pg = ctx.new_page()
    pg.set_default_timeout(15000)
    yield pg
    ctx.close()
