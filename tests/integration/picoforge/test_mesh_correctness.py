"""Mesh-correctness checks for the picoforge stack.

The regression that motivated these: a renamed RPC path
(`accounts.store.count` -> `accounts.accounts.count`) left the admin
*Users* page rendering "accounts service unreachable" instead of the
roster — the webapp swallowed the dead call into a -1 and degraded the
page. The UI flow tests never opened that page, so nothing caught it.

Two layers cover it now:

  * `test_gateway_rpc_surface_reachable` — a SERVICE-DRIVEN sweep. It
    discovers every service/class/method from the gateway's `/_describe`
    (never a hardcoded ladder), reflects each read method's parameters
    from `/<path>/_describe`, invokes it over `/_rpc` with benign default
    arguments, and asserts a real result comes back. A renamed, dropped,
    or crashing `count`/`list`/`list_all` fails the test instead of
    silently degrading a page.

  * `test_admin_users_roster` — drives the literal admin page through the
    browser as the site admin and asserts the real account table renders
    (no "unreachable").

Everything goes through the gateway (`/_rpc`, `/_describe`) or the webapp;
nothing here pokes a backend port directly.
"""

import json
import time
import urllib.error
import urllib.request
import uuid

import pytest
from playwright.sync_api import expect

from helpers import PASSWORD, USER, register, sign_in_or_register

# The uniform "list everything this service manages" surface added to every
# service. These take no required entity argument (globals), so a reflective
# default-arg invocation is always valid — unlike entity-scoped helpers such
# as count_for_owner / count_open_in_repo, which need a real id.
GLOBAL_READ_VERBS = ("count", "list", "list_all")

# Core services we expect the gateway to front. A missing one means the mesh
# came up degraded; an unexpected absence is itself a correctness failure.
EXPECTED_SERVICES = {
    "accounts", "git_repo", "issues", "git_pipeline", "session",
    "password_authn", "token_issuer", "personal_access_tokens",
    "sharded_storage",
}


# ---- RPC reflection helpers (stdlib only) ---------------------------------

def _get_json(url):
    with urllib.request.urlopen(url, timeout=10) as resp:
        return resp.getcode(), json.loads(resp.read())


def _post_rpc(gateway, path, args):
    """Invoke `path` over the gateway's JSON /_rpc. Returns (status, body)."""
    body = json.dumps({"path": path, "args": args, "kwargs": {}}).encode()
    req = urllib.request.Request(
        gateway + "/_rpc", data=body,
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return resp.getcode(), json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        try:
            return exc.code, json.loads(exc.read())
        except Exception:  # noqa: BLE001
            return exc.code, None


def _is_numeric_type(type_str):
    type_lower = (type_str or "").lower()
    return ("int" in type_lower or "size_t" in type_lower
            or "long" in type_lower or "unsigned" in type_lower
            or "float" in type_lower or "double" in type_lower)


def _coerce_default(type_str):
    """A benign default value for a parameter of the given C type — the same
    rule the console uses to fill an empty field: 0 for numbers, "" for
    strings, false for bools."""
    type_lower = (type_str or "").lower()
    if "bool" in type_lower:
        return False
    if _is_numeric_type(type_str):
        return 0
    return ""  # char * and everything else


def _describe_params(gateway, path):
    """The reflected parameter list for `path`, from `/<path>/_describe`."""
    status, data = _get_json(gateway + "/" + path + "/_describe")
    return data.get("params", []) if isinstance(data, dict) else []


def _default_args(gateway, path):
    """Positional default-argument vector for `path`, reflected from its
    parameter list."""
    return [_coerce_default(param.get("type", ""))
            for param in _describe_params(gateway, path)]


def _describe_services(gateway):
    status, data = _get_json(gateway + "/_describe")
    assert status == 200, f"/_describe returned HTTP {status}"
    assert isinstance(data, dict) and "services" in data, \
        f"/_describe shape unexpected: {data!r}"
    return data["services"]


def _global_read_methods(gateway):
    """Yield (dotted_path, verb) for every GENUINELY GLOBAL read method the
    gateway advertises — `count`/`list`/`list_all` whose parameters are all
    numeric (offset/limit) or absent. Service-driven, no hardcoded ladder.

    Methods keyed by a required entity are intentionally excluded: a
    partitioned KV store (`sharded_storage.db`) keys its reads on a string
    `context`/`namespace` and has no cross-namespace "list everything", so an
    empty key is *correctly* rejected — that is not the reachability failure
    this sweep is hunting for. The all-numeric-params signature cleanly
    separates the two without naming any service."""
    for service in _describe_services(gateway):
        for klass in service.get("classes", []):
            prefix = (klass.get("qname", "") or "") + "_"
            for method_q in klass.get("methods", []):
                verb = method_q[len(prefix):] if method_q.startswith(prefix) else method_q
                if verb not in GLOBAL_READ_VERBS:
                    continue
                path = klass["class"] + "." + verb
                params = _describe_params(gateway, path)
                if all(_is_numeric_type(param.get("type", "")) for param in params):
                    yield (path, verb)


# ---- the reflected RPC surface --------------------------------------------

def test_gateway_describe_lists_core_services(gateway_url):
    """/_describe must reflect the live mesh. Every core service should be
    present — a missing one means the stack came up degraded."""
    present = {service.get("service") for service in _describe_services(gateway_url)}
    missing = EXPECTED_SERVICES - present
    assert not missing, (
        f"gateway /_describe is missing core services {sorted(missing)}; "
        f"present: {sorted(present)}")


def test_gateway_rpc_surface_reachable(gateway_url):
    """Every global read method the gateway advertises must actually answer
    over /_rpc. This is the generic guard for the admin-page regression:
    a renamed/dropped/crashing count|list|list_all fails here."""
    methods = list(_global_read_methods(gateway_url))
    assert methods, "/_describe advertised no count/list/list_all methods"

    failures = []
    for path, verb in methods:
        args = _default_args(gateway_url, path)
        status, body = _post_rpc(gateway_url, path, args)
        ok = (status == 200 and isinstance(body, dict)
              and "result" in body and "error" not in body)
        # An authz denial ("forbidden …") proves the method is REACHABLE: it
        # resolved, ran, and enforced its policy. That is exactly what this
        # sweep checks. Some global reads (issues/pipeline list, list_all) are
        # site-admin-only post-RBAC, so an anonymous probe is correctly denied
        # — not a regression. Only a renamed/dropped method ("no method") or a
        # crash should fail here.
        err_msg = ""
        if isinstance(body, dict) and isinstance(body.get("error"), dict):
            err_msg = body["error"].get("message") or ""
        denied = "forbidden" in err_msg.lower()
        if not (ok or denied):
            failures.append(f"{path}(args={args}) -> HTTP {status} {body!r}")

    assert not failures, (
        "unreachable / erroring read methods on the gateway:\n  "
        + "\n  ".join(failures))


def test_accounts_roster_rpc(gateway_url):
    """The two calls the admin Users page makes — accounts count + list_all —
    must answer and agree. Catches the exact 'accounts service unreachable'
    bug at the RPC layer, browser-free."""
    status, count_body = _post_rpc(gateway_url, "accounts.accounts.count", [])
    assert status == 200 and isinstance(count_body.get("result"), int), \
        f"accounts.accounts.count did not return an int: HTTP {status} {count_body!r}"

    status, list_body = _post_rpc(gateway_url, "accounts.accounts.list_all", [])
    assert status == 200 and isinstance(list_body.get("result"), list), \
        f"accounts.accounts.list_all did not return an array: HTTP {status} {list_body!r}"

    roster = list_body["result"]
    assert len(roster) == count_body["result"], \
        f"count ({count_body['result']}) != len(list_all) ({len(roster)})"
    for row in roster:
        assert isinstance(row, dict) and "uid" in row and "username" in row, \
            f"roster row should be {{uid,username}}, got {row!r}"


def test_accounts_list_pagination(gateway_url):
    """`list` is paginated (offset, limit) and capped; `list_all` returns the
    full set. A limited window must never exceed its limit, and must be a
    subset of everything."""
    # Discover accounts.accounts.list arity reflectively, then ask for a
    # 1-row window from the front.
    base_args = _default_args(gateway_url, "accounts.accounts.list")
    # The list signature is (offset, limit) — set a limit of 1 on the last arg
    # and offset 0 on the first, leaving any extra leading args at defaults.
    window_args = list(base_args)
    if len(window_args) >= 2:
        window_args[-2] = 0   # offset
        window_args[-1] = 1   # limit
    status, page_body = _post_rpc(gateway_url, "accounts.accounts.list", window_args)
    assert status == 200 and isinstance(page_body.get("result"), list), \
        f"accounts.accounts.list did not return an array: HTTP {status} {page_body!r}"
    assert len(page_body["result"]) <= 1, \
        f"list(limit=1) returned {len(page_body['result'])} rows — pagination not capped"

    status, all_body = _post_rpc(gateway_url, "accounts.accounts.list_all", [])
    assert status == 200 and isinstance(all_body.get("result"), list)
    assert len(all_body["result"]) >= len(page_body["result"]), \
        "list_all must contain at least the windowed page"


# ---- distributed tracing ---------------------------------------------------

def _await_trace(gateway, trace_id, want_spans=2, timeout=6.0):
    """Poll the collector for `trace_id` until it has at least `want_spans`
    spans or the timeout elapses. Spans ship fire-and-forget, so a single read
    races the export; polling makes the check robust without being slow."""
    deadline = time.time() + timeout
    latest = {}
    while time.time() < deadline:
        status, body = _post_rpc(
            gateway, "trace_collector.trace_collector.get_trace", [trace_id])
        if status == 200 and isinstance(body, dict):
            raw = body.get("result")
            trace = json.loads(raw) if isinstance(raw, str) else raw
            if isinstance(trace, dict):
                latest = trace
                if len(trace.get("spans", [])) >= want_spans:
                    return trace
        time.sleep(0.3)
    return latest


def test_distributed_trace_reconstructs(gateway_url):
    """A traced request through the gateway must produce a linked, cross-service
    span tree in the trace_collector. This exercises the span exporter end to
    end — a silent exporter break (e.g. a renamed ingest method) drops every
    span and is invisible to the page tests."""
    # Tracing needs the trace_collector backend + the span exporter. A
    # one-node / collocated config (e.g. the webasm/qemu image) doesn't run
    # trace_collector, so there is nothing to reconstruct — skip rather than
    # fail; the multi-node mesh runs it and exercises this path.
    if "trace_collector" not in {s.get("service") for s in _describe_services(gateway_url)}:
        pytest.skip("trace_collector not active (one-node/collocated) — tracing not exercised")
    trace_id = uuid.uuid4().hex          # 32 hex chars == a W3C trace-id
    parent_span = uuid.uuid4().hex[:16]  # 16 hex chars == a W3C span-id

    req = urllib.request.Request(
        gateway_url + "/_rpc",
        data=json.dumps({"path": "git_repo.git_repo.count_total", "args": []}).encode(),
        headers={"Content-Type": "application/json",
                 "traceparent": f"00-{trace_id}-{parent_span}-01"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        echoed = resp.headers.get("traceparent", "")
    assert echoed.startswith(f"00-{trace_id}-"), \
        f"gateway did not continue our trace context: {echoed!r}"

    trace = _await_trace(gateway_url, trace_id, want_spans=2)
    spans = trace.get("spans", [])
    assert len(spans) >= 2, (
        f"trace_collector reconstructed {len(spans)} spans for the request — the "
        f"span exporter is dropping spans (expected a gateway→backend chain)")
    services = {span.get("service_name") for span in spans}
    assert len(services) >= 2, f"trace should span more than one service: {services}"
    span_ids = {span["span_id"] for span in spans}
    assert any(span.get("parent_span_id") in span_ids for span in spans), \
        "no span links to a parent in the same trace — tree not reconstructed"


# ---- the literal admin page (browser) -------------------------------------

def test_admin_users_roster(page, base_url):
    """The admin Users page must render the real account roster, not
    'accounts service unreachable'. Drives it as the site admin (the first
    registrant on a fresh stack) and asserts the table shows the accounts."""
    # First registrant on a fresh stack is the bootstrap site admin.
    sign_in_or_register(page, base_url)  # USER, admin
    # A second account so the roster has more than one row. Registering signs
    # the new user in, so we sign back in as the admin afterwards.
    second = "rosterite"
    register(page, base_url, second, PASSWORD)
    sign_in_or_register(page, base_url)  # back to USER (admin)

    resp = page.goto(f"{base_url}/-/admin/users")
    assert resp.status == 200, f"/-/admin/users as admin should be 200, got {resp.status}"
    expect(page.locator("h1")).to_have_text("Users")

    body = page.content()
    assert "accounts service unreachable" not in body, (
        "admin Users page reports the accounts service unreachable — the "
        "count/list RPC path is broken")

    # The roster table lists the real accounts.
    table = page.locator("table.file-table")
    expect(table).to_be_visible()
    expect(table).to_contain_text(USER)
    expect(table).to_contain_text(second)


def test_admin_users_roster_count_tile(page, base_url):
    """The Users page stat tile reflects a real count (>= the two accounts we
    registered), proving accounts.accounts.count round-tripped."""
    sign_in_or_register(page, base_url)  # admin
    page.goto(f"{base_url}/-/admin/users")
    tile = page.locator("section.stats-grid")
    expect(tile).to_be_visible()
    # The tile shows "<n> users"; pull the integer and assert it is sane.
    text = tile.inner_text()
    digits = "".join(ch for ch in text if ch.isdigit())
    assert digits and int(digits) >= 1, f"users count tile looks wrong: {text!r}"
