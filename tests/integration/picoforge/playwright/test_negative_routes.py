"""Negative routing and anonymous-access boundaries in the webapp: unknown
commands and repo verbs 404, anonymous visitors are bounced from protected
pages, and an anonymous mutation never creates anything.

The happy path only ever walks valid, authenticated routes. A regression that
turned a 404 into a 200 (rendering a stub for a non-existent page) or dropped
an anonymous redirect (leaking a data page to a signed-out visitor) would slip
straight past it. These assert the deny side.

All requests go browser/urllib -> picoforge-webapp; nothing pokes a backend
port directly.
"""

import re
import urllib.error
import urllib.request

from playwright.sync_api import expect

from helpers import sign_in_or_register, signed_in, USER
from forge import ensure_repo


def _no_redirect_post(url, data=b""):
    """POST without following redirects. Returns (status, location)."""

    class _Stop(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *a, **k):
            return None

    opener = urllib.request.build_opener(_Stop)
    req = urllib.request.Request(url, method="POST", data=data)
    try:
        resp = opener.open(req, timeout=10)
        return resp.getcode(), resp.headers.get("Location")
    except urllib.error.HTTPError as e:
        return e.code, e.headers.get("Location")


# ---- 404s (signed in, so the miss is a routing miss, not an auth bounce) ----

def test_unknown_command_404(page, base_url):
    """An unknown /-/<command> returns 404, not a rendered stub."""
    sign_in_or_register(page, base_url)
    resp = page.goto(f"{base_url}/-/totally-not-a-page")
    assert resp.status == 404, f"unknown command should 404, got {resp.status}"


def test_unknown_repo_verb_404(page, base_url):
    """A project sub-page with a verb that is not in the route table 404s."""
    sign_in_or_register(page, base_url)
    repo = "verb404-demo"
    ensure_repo(page, base_url, USER, repo)
    resp = page.goto(f"{base_url}/{USER}/{repo}/-/nonsense")
    assert resp.status == 404, f"unknown repo verb should 404, got {resp.status}"


# ---- anonymous bounces (fresh page context == no session) -------------------

def test_anonymous_repos_redirects_to_login(page, base_url):
    """An anonymous visit to the projects page redirects to /-/login."""
    page.goto(f"{base_url}/-/repos")
    expect(page).to_have_url(re.compile(r"/-/login$"))
    assert not signed_in(page)


def test_anonymous_groups_redirects_to_login(page, base_url):
    """An anonymous visit to the groups page redirects to /-/login."""
    page.goto(f"{base_url}/-/groups")
    expect(page).to_have_url(re.compile(r"/-/login$"))


def test_anonymous_namespace_path_redirects_to_login(page, base_url):
    """A bare namespace path is member-gated data; an anonymous caller is sent
    to /-/login rather than being served the landing page."""
    page.goto(f"{base_url}/somebody")
    expect(page).to_have_url(re.compile(r"/-/login$"))


def test_anonymous_dashboard_issues_leaks_no_repos(page, base_url):
    """The issues dashboard renders for anyone, but an anonymous caller has no
    accessible repositories — it must show the empty state, never another
    user's repos."""
    page.goto(f"{base_url}/-/dashboard/issues")
    expect(page.locator("h1")).to_have_text("Issues")
    expect(page.locator("text=No repositories.")).to_be_visible()
    # And the chrome shows a signed-out visitor (Sign in, not a username).
    expect(page.locator("header.topbar a[href='/-/login']")).to_be_visible()


def test_anonymous_cannot_create_repo(base_url):
    """An anonymous POST to create a repo is refused with a redirect to
    /-/login and never creates anything."""
    status, loc = _no_redirect_post(f"{base_url}/-/repos/new")
    assert status in (302, 303), f"anon repo create should redirect, got {status}"
    assert loc and "/login" in loc, f"should redirect to /login, got {loc!r}"
