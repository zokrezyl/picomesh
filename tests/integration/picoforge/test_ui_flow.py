"""End-to-end UI + authorization flow for the picoforge webapp, driven
through a real browser (Playwright + system Chrome).

The happy path clicks through the app like a user: sign up, create a
repository, create + edit a file in the Monaco editor, file an issue,
enqueue a pipeline run, and sign out. The authorization tests prove the
boundary the gateway/webapp now enforce:

  * admin space is gated BEFORE rendering (anonymous → /login, signed-in
    non-admin → 403),
  * mutations require a real session (no fallback identity),
  * ownership comes from the session, never the picomesh-uname cookie.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc ->
backends; nothing here pokes a backend port directly.
"""

import re
import urllib.error
import urllib.request

from playwright.sync_api import expect

from helpers import (
    USER,
    PASSWORD,
    sign_in_or_register,
    register as _register,
    login as _login,
    signed_in as _signed_in,
)

REPO = "demo-site"
FILE_PATH = "README.md"
FILE_V1 = "# Demo site\n\nhello from the integration test\n"
EDIT_MARKER = "edited by the integration test"


# ---- helpers --------------------------------------------------------------

def _request(url, *, method="GET", data=None, cookies=None):
    """Issue a request WITHOUT following redirects. Returns (status, location)."""

    class _NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *a, **k):  # noqa: D401 — suppress redirects
            return None

    opener = urllib.request.build_opener(_NoRedirect)
    req = urllib.request.Request(url, method=method, data=data)
    if cookies:
        req.add_header("Cookie", cookies)
    try:
        resp = opener.open(req, timeout=10)
        return resp.getcode(), resp.headers.get("Location")
    except urllib.error.HTTPError as e:
        return e.code, e.headers.get("Location")


def monaco_focus(page):
    """Wait for the Monaco editor to render and focus its text area, the
    same way a user clicks into the editor before typing."""
    page.wait_for_selector("#editor .monaco-editor", timeout=20000)
    page.wait_for_selector("#editor textarea.inputarea", timeout=20000)
    page.click("#editor .monaco-editor .view-lines")


def monaco_type(page, text):
    """Type `text` into the editor with real keystrokes (no JS injection)."""
    monaco_focus(page)
    page.keyboard.type(text)


def monaco_text(page):
    """Read what the editor is actually showing the user (rendered lines).
    Monaco renders spaces as non-breaking spaces in the DOM, so normalize
    them back so substring assertions match the typed text."""
    page.wait_for_selector("#editor .view-lines", timeout=20000)
    return page.locator("#editor .view-lines").inner_text().replace("\xa0", " ")


# ---- the happy path -------------------------------------------------------

def test_full_ui_flow(page, base_url):
    # 1. Sign up (or in) through the form. On a fresh stack USER is the first
    #    registrant → the bootstrap site admin, so the Admin link is present.
    sign_in_or_register(page, base_url)
    expect(page.locator("header.topbar a[href='/-/admin']")).to_have_count(1)

    # 2. Create a repository via the dedicated /-/repos/new page. Scope the
    #    click to the form — the signed-in topbar also has a submit button
    #    (Sign out), so a bare button[type=submit] is ambiguous.
    page.click("a[href='/-/repos/new']")
    page.wait_for_url(re.compile(r"/-/repos/new$"))
    page.fill("input[name=name]", REPO)
    page.click("form[action='/-/repos/new'] button[type=submit]")
    page.wait_for_load_state("networkidle")
    # Repo creation lands on the new repo's tree page (GitHub/GitLab style).
    expect(page).to_have_url(re.compile(rf"/{re.escape(USER)}/{REPO}/-/tree$"))

    # 3. The new repo also shows up in the repos list, linking to the repo's
    #    tree page (/<user>/<repo>/-/tree).
    page.goto(f"{base_url}/-/repos")
    repo_link = page.locator(f"a[href='/{USER}/{REPO}/-/tree']").first
    expect(repo_link).to_be_visible()

    # 4. Open the repository browser.
    repo_link.click()
    page.wait_for_load_state("networkidle")
    expect(page).to_have_url(re.compile(rf"/{re.escape(USER)}/{REPO}/-/tree$"))
    # The project sub-nav (Code / Issues / Pipelines / Settings) is present.
    expect(page.locator("nav.project-tabs")).to_be_visible()

    # 5. Create a file: click "New file", type the path, TYPE the content
    #    into the Monaco editor with real keystrokes, then Commit.
    page.locator("a.btn:has-text('New file')").first.click()
    page.wait_for_url(re.compile(r"/new$"))
    page.fill("input[name=path]", FILE_PATH)
    monaco_type(page, FILE_V1)
    page.click("#f button[type=submit]")  # Commit (the editor form)
    page.wait_for_load_state("networkidle")

    # 6. The file is now listed in the repo tree, linking to its editor.
    file_link = page.locator(f"a[href*='/edit?path={FILE_PATH}']").first
    expect(file_link).to_be_visible()

    # 7. Click the file open — the editor shows the committed content.
    file_link.click()
    page.wait_for_url(re.compile(r"/edit\?path="))
    assert "hello from the integration test" in monaco_text(page)

    # 8. Edit the file: jump to the end and TYPE a marker line, then Commit.
    monaco_focus(page)
    page.keyboard.press("Control+End")
    page.keyboard.type("\n" + EDIT_MARKER + "\n")
    page.click("#f button[type=submit]")
    page.wait_for_load_state("networkidle")

    # 9. Re-open the file by clicking it again — the typed edit persisted.
    page.locator(f"a[href*='/edit?path={FILE_PATH}']").first.click()
    page.wait_for_url(re.compile(r"/edit\?path="))
    assert EDIT_MARKER in monaco_text(page), "edit did not round-trip via git_repo"

    # Back to the repo via the editor's Cancel link, so we can use the tabs.
    page.click("a.btn:has-text('Cancel')")
    page.wait_for_url(re.compile(rf"/{re.escape(USER)}/{REPO}/-/tree$"))

    # 10. Issues: navigate via the project tab, file one by clicking the button.
    page.click("nav.project-tabs a:has-text('Issues')")
    page.wait_for_url(re.compile(r"/issues$"))
    page.click("form[action$='/-/issues/new'] button[type=submit]")
    page.wait_for_load_state("networkidle")
    expect(page.locator("text=/[1-9][0-9]* open issue/")).to_be_visible()

    # 11. Pipelines: navigate via the project tab, enqueue by clicking.
    page.click("nav.project-tabs a:has-text('Pipelines')")
    page.wait_for_url(re.compile(r"/runs$"))
    # The panel's "Run pipeline" button is .primary (the project-header one
    # is .btn); target the panel action specifically.
    page.click("button.primary:has-text('Run pipeline')")
    page.wait_for_load_state("networkidle")
    queued = page.locator("table.pipeline-table tr", has_text="queued").locator("td").last
    assert int(queued.inner_text().strip()) >= 1, "queued run count should be >= 1"

    # 12. Sign out by clicking the topbar button — back to the sign-in page.
    page.click("header.topbar form[action='/-/logout'] button[type=submit]")
    page.wait_for_url(re.compile(r"/-/login$"))
    expect(page.locator("h1")).to_have_text("Sign in")


# ---- authorization boundary ----------------------------------------------

def test_anonymous_redirected_from_admin(page, base_url):
    """An anonymous visitor must never see admin UI — the webapp resolves
    identity from the session and bounces an unauthenticated /admin to
    /login before rendering anything."""
    page.goto(f"{base_url}/-/admin")
    expect(page).to_have_url(re.compile(r"/login$"))
    assert not _signed_in(page)


def test_non_admin_forbidden_from_admin(page, base_url):
    """A signed-in but non-admin user must get a 403 from admin space, not
    the admin shell. The first registrant bootstraps as site owner, so a
    second account is guaranteed non-admin."""
    sign_in_or_register(page, base_url)  # USER — site owner on a fresh stack
    second = "regular"
    _register(page, base_url, second, PASSWORD)
    if not _signed_in(page):
        _login(page, base_url, second, PASSWORD)
    assert _signed_in(page), "second account should be signed in"

    resp = page.goto(f"{base_url}/-/admin")
    assert resp.status == 403, f"non-admin /admin should be 403, got {resp.status}"
    expect(page.locator("h1")).to_have_text("Forbidden")

    # The nav must not even advertise admin access to a non-admin: no Admin
    # link anywhere in the topbar on a normal page.
    page.goto(f"{base_url}/-/repos")
    assert page.locator("header.topbar a[href='/-/admin']").count() == 0, \
        "non-admin must not see an Admin link in the topbar"


def test_repo_owner_from_session_not_cookie(page, base_url):
    """Ownership must come from the session, not the picomesh-uname cookie.
    Forge a different uname cookie and confirm a created repo is still owned
    by the real session user."""
    sign_in_or_register(page, base_url)  # signed in as USER
    page.context.add_cookies([{
        "name": "picomesh-uname", "value": "attacker", "url": base_url}])

    page.goto(f"{base_url}/-/repos/new")
    page.fill("input[name=name]", "cookie-test")
    page.click("form[action='/-/repos/new'] button[type=submit]")
    page.wait_for_load_state("networkidle")
    # Repo creation lands on the new repo's tree page, under the SESSION user
    # (not the forged cookie) — the URL itself proves ownership-from-session.
    expect(page).to_have_url(re.compile(rf"/{re.escape(USER)}/cookie-test/-/tree$"))

    # Confirm on the repos list: owned by the real session user, NOT the cookie.
    page.goto(f"{base_url}/-/repos")
    expect(page.locator(f"a[href='/{USER}/cookie-test/-/tree']")).to_be_visible()
    assert page.locator("a[href='/attacker/cookie-test/-/tree']").count() == 0, \
        "repo must not be created under the forged picomesh-uname cookie"


def test_anonymous_cannot_create_issue(base_url):
    """A mutation with no session must be refused (redirect to /login), never
    attributed to a fallback uid."""
    status, loc = _request(
        f"{base_url}/{USER}/{REPO}/-/issues/new", method="POST", data=b"")
    assert status in (302, 303), f"anon issue create should redirect, got {status}"
    assert loc and "/login" in loc, f"should redirect to /login, got {loc!r}"


def test_anonymous_cannot_enqueue_run(base_url):
    """A pipeline enqueue with no session must be refused, not run as a
    fallback identity."""
    status, loc = _request(
        f"{base_url}/{USER}/{REPO}/-/runs/new", method="POST", data=b"")
    assert status in (302, 303), f"anon run enqueue should redirect, got {status}"
    assert loc and "/login" in loc, f"should redirect to /login, got {loc!r}"


def test_gateway_serves_no_html(gateway_url):
    """The webapp owns pages; the gateway must 404 HTML GETs. Cross-checks
    the gh#5 invariant from the browser's side, via urllib. Targets the
    gateway_url fixture (PICOFORGE_GATEWAY_URL) so it works against both the
    multi-node mesh and a one-node / collocated gateway on any port."""
    for path in ("/", "/-/login", "/-/repos"):
        try:
            code = urllib.request.urlopen(gateway_url + path, timeout=8).getcode()
        except urllib.error.HTTPError as e:
            code = e.code
        assert code == 404, f"gateway {path} returned {code}, expected 404 (API-only)"


def test_gateway_whoami_anonymous(gateway_url):
    """The gateway's /_whoami returns anonymous claims (uid 0) when no
    session token is presented — and never leaks a JWT."""
    import json
    body = urllib.request.urlopen(gateway_url + "/_whoami", timeout=8).read()
    claims = json.loads(body)
    assert claims.get("uid") == 0, f"anon /_whoami should be uid 0, got {claims}"
    assert claims.get("is_admin") in (False, 0), "anon must not be admin"
