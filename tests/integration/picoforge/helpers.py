"""Shared browser helpers for the picoforge UI integration tests.

These drive the real webapp the way a human does — fill the form, click
submit, wait for the page. They're shared by `test_ui_flow.py` (the happy
path) and `test_mesh_correctness.py` (the admin/RPC correctness checks) so
there is exactly one definition of "sign in" / "register".
"""

import re

from playwright.sync_api import expect

USER = "uitester"
PASSWORD = "hunter2"


def signed_in(page) -> bool:
    """True when the app shell shows the signed-in controls. The sign-out
    form lives in the topbar only for an authenticated user."""
    return page.locator("header.topbar form[action='/logout']").count() > 0


def register(page, base_url, user, password):
    """Sign `user` up via the /register form. On success the webapp signs
    them in and redirects to /repos."""
    page.goto(f"{base_url}/register")
    page.fill("input[name=username]", user)
    page.fill("input[name=password]", password)
    page.click("form[action='/register'] button[type=submit]")
    page.wait_for_load_state("networkidle")


def login(page, base_url, user, password):
    """Sign `user` in via the /login form."""
    page.goto(f"{base_url}/login")
    page.fill("input[name=username]", user)
    page.fill("input[name=password]", password)
    page.click("form[action='/login'] button[type=submit]")
    page.wait_for_load_state("networkidle")


def sign_in_or_register(page, base_url, user=USER, password=PASSWORD):
    """Register `user`; if the account already exists (re-run against a
    persistent stack), sign in instead. Leaves the browser on /repos,
    signed in."""
    register(page, base_url, user, password)
    if not signed_in(page):
        login(page, base_url, user, password)
    expect(page).to_have_url(re.compile(r"/repos$"))
    expect(page.locator("h1")).to_have_text("Repositories")
    assert signed_in(page), "topbar should show signed-in controls after auth"
