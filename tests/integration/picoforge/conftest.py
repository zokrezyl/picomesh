"""Pytest fixtures for the picoforge UI integration tests.

Two things are provisioned:

  * `base_url` — a live picoforge webapp to drive. By default the fixture
    brings the WHOLE stack up itself (control parent -> reconcile spawns the
    gateway + backends -> picoforge-webapp on :8081) against a fresh,
    wiped /tmp/picoforge, and tears it all down afterwards. Set
    PICOFORGE_WEBAPP_URL=http://host:port to instead drive an already-running
    stack (e.g. a dev `stack-up.sh`) and skip lifecycle management.

  * `page` — a Playwright page backed by the system Google Chrome
    (channel="chrome"), so no `playwright install` browser download is needed.

The whole point is to exercise the REAL browser UI end to end, the same way
a human clicks through it — not the gateway API directly.
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
CONFIG = os.path.join(REPO_ROOT, "scenarios", "picoforge", "picoforge.yaml")
STATIC = os.path.join(REPO_ROOT, "scenarios", "picoforge", "webapp", "static")
CTRL, WEB, SIDE = 8800, 8080, 8081


def _port_open(port, host="127.0.0.1"):
    with socket.socket() as s:
        s.settimeout(0.3)
        try:
            s.connect((host, port))
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


def _post_json(url, obj):
    data = json.dumps(obj).encode()
    req = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read())


def _reap():
    """Best-effort kill of any lingering stack so ports are free."""
    for pat in ("picoforge-webapp", "picomesh.*serve"):
        subprocess.run(["pkill", "-9", "-f", pat],
                       stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)


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

    _reap()
    time.sleep(1)
    subprocess.run(["rm", "-rf", "/tmp/picoforge"])
    os.makedirs("/tmp/picoforge", exist_ok=True)

    parent = subprocess.Popen(
        [PICOMESH, "--config-file", CONFIG, "--frontend", "yhttp",
         "--host", "127.0.0.1", "--port", str(CTRL), "serve"],
        stdout=open("/tmp/picoforge/itest-parent.log", "w"),
        stderr=subprocess.STDOUT)
    webapp = None
    try:
        assert _wait_port(CTRL, 15), "control parent did not bind"
        handle = _post_json(f"http://127.0.0.1:{CTRL}/create",
                            {"class": "mesh_store"})["handle"]
        _post_json(f"http://127.0.0.1:{CTRL}/invoke",
                   {"method": "mesh_store_reconcile_from_config",
                    "handle": handle, "args": []})
        assert _wait_port(WEB, 30), "gateway did not bind"
        time.sleep(1.5)  # let backends finish binding before first call

        webapp = subprocess.Popen(
            [WEBAPP, "--gateway-url", f"http://127.0.0.1:{WEB}",
             "--host", "127.0.0.1", "--port", str(SIDE), "--static", STATIC],
            stdout=open("/tmp/picoforge/itest-webapp.log", "w"),
            stderr=subprocess.STDOUT)
        assert _wait_port(SIDE, 15), "webapp did not bind"

        yield f"http://127.0.0.1:{SIDE}"
    finally:
        if webapp:
            webapp.terminate()
        parent.terminate()
        time.sleep(0.5)
        _reap()  # the reconcile-spawned backends are not our children


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
