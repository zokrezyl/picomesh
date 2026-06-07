"""The authentication lifecycle through the browser: the login/register page
shapes, wrong-password and duplicate-registration error states, and that
logout actually tears the session down.

The top-level suite asserts the redirect boundaries (anon → /login, non-admin
→ 403) but never the negative auth paths: a bad password, a taken username, or
whether signing out really clears the session cookie. Those are exactly the
spots where an auth regression hides — a form that 'succeeds' on a wrong
password, or a logout that leaves the cookie live.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, register, login, signed_in, USER, PASSWORD


def test_login_page_renders(page, base_url):
    """GET /-/login serves the sign-in form with username + password fields."""
    page.goto(f"{base_url}/-/login")
    expect(page.locator("h1")).to_have_text("Sign in")
    form = page.locator("form[action='/-/login']")
    expect(form.locator("input[name=username]")).to_be_visible()
    expect(form.locator("input[name=password]")).to_be_visible()


def test_register_page_renders(page, base_url):
    """GET /-/register serves the create-account form."""
    page.goto(f"{base_url}/-/register")
    expect(page.locator("h1")).to_have_text("Create account")
    form = page.locator("form[action='/-/register']")
    expect(form.locator("input[name=username]")).to_be_visible()
    expect(form.locator("input[name=password]")).to_be_visible()


def test_login_wrong_password_shows_error(page, base_url):
    """Signing in with the wrong password re-renders the login page with an
    error and leaves the visitor signed out — never a silent success."""
    sign_in_or_register(page, base_url)  # make sure USER exists
    login(page, base_url, USER, "definitely-not-the-password")
    expect(page.locator("h1")).to_have_text("Sign in")
    expect(page.locator("p.error")).to_contain_text("invalid username or password")
    assert not signed_in(page), "a wrong password must not establish a session"


def test_duplicate_registration_shows_error(page, base_url):
    """Registering a username that already exists is refused with an error,
    not a second account."""
    sign_in_or_register(page, base_url)  # USER now certainly exists
    # Register USER again from a clean (cookie-less) context page.
    register(page, base_url, USER, PASSWORD)
    expect(page.locator("h1")).to_have_text("Create account")
    expect(page.locator("p.error")).to_contain_text("already")
    assert not signed_in(page)


def test_logout_clears_session(page, base_url):
    """Signing out returns to /-/login and the session is gone — a protected
    page now bounces back to login instead of rendering."""
    sign_in_or_register(page, base_url)
    page.click("header.topbar form[action='/-/logout'] button[type=submit]")
    page.wait_for_url(re.compile(r"/-/login$"))
    assert not signed_in(page)

    # The session is really gone: a protected page redirects to login.
    page.goto(f"{base_url}/-/repos")
    expect(page).to_have_url(re.compile(r"/-/login$"))


def test_register_then_logout_then_login_roundtrip(page, base_url):
    """A full credential round-trip for a brand-new account: register (auto
    signed in) → logout → log back in with the same credentials."""
    sign_in_or_register(page, base_url)  # ensure USER is the site owner first
    user = "roundtripper"
    register(page, base_url, user, PASSWORD)
    if not signed_in(page):
        login(page, base_url, user, PASSWORD)
    assert signed_in(page)

    page.click("header.topbar form[action='/-/logout'] button[type=submit]")
    page.wait_for_url(re.compile(r"/-/login$"))

    login(page, base_url, user, PASSWORD)
    expect(page).to_have_url(re.compile(r"/-/repos$"))
    assert signed_in(page)
