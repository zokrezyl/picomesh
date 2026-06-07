"""Small browser helpers shared by the playwright/ page tests.

These sit on top of the parent `helpers.py` (sign-in/register) and add the
one extra primitive the page tests need that the happy-path suite does not
expose on its own: idempotently making sure a named repository exists so a
page that lists/branches off a repo has something real to render.

Everything drives the real webapp UI (browser -> picoforge-webapp -> gateway
/_rpc -> backends); nothing here pokes a backend port directly.
"""

import re

from playwright.sync_api import expect

from helpers import register, login, signed_in


def ensure_repo(page, base_url, user, name):
    """Make sure `user` owns a repo called `name`, creating it through the
    /-/repos/new form if it is not already listed. Idempotent so the page
    tests work on a fresh session stack AND on a re-run against a persistent
    one. Leaves the browser on the repos list."""
    page.goto(f"{base_url}/-/repos")
    tree_href = f"/{user}/{name}/-/tree"
    if page.locator(f"a[href='{tree_href}']").count() == 0:
        page.goto(f"{base_url}/-/repos/new")
        page.fill("input[name=name]", name)
        # The namespace field defaults to the user's personal namespace, which
        # is exactly where we want it — leave it.
        page.click("form[action='/-/repos/new'] button[type=submit]")
        expect(page).to_have_url(
            re.compile(rf"/{re.escape(user)}/{re.escape(name)}/-/tree$"))
        page.goto(f"{base_url}/-/repos")
    expect(page.locator(f"a[href='{tree_href}']").first).to_be_visible()


def ensure_group(page, base_url, slug):
    """Make sure a top-level group `slug` exists (owned by the signed-in
    user), creating it via the /-/groups create form if absent. Idempotent.
    Leaves the browser on /-/groups."""
    page.goto(f"{base_url}/-/groups")
    manage_href = f"/-/groups/{slug}"
    if page.locator(f"a[href='{manage_href}']").count() == 0:
        page.fill("form[action='/-/groups/create'] input[name=slug]", slug)
        page.click("form[action='/-/groups/create'] button[type=submit]")
        page.wait_for_load_state("networkidle")
        page.goto(f"{base_url}/-/groups")


def open_second_user(page, base_url, user, password):
    """Open a SEPARATE browser context (independent cookie jar) and sign a
    distinct `user` in there, returning that user's page. Used for multi-user
    RBAC isolation tests where two real sessions must coexist. The caller must
    close `page2.context` when done. Because the site owner is whoever
    registered first, callers should already have signed the admin (USER) in on
    `page` before opening a second user, so the second account is non-admin."""
    ctx = page.context.browser.new_context()
    page2 = ctx.new_page()
    page2.set_default_timeout(15000)
    register(page2, base_url, user, password)
    if not signed_in(page2):
        login(page2, base_url, user, password)
    assert signed_in(page2), f"second user {user!r} should be signed in"
    return page2


def file_an_issue(page, base_url, user, repo):
    """File one issue on `user/repo` through the project Issues tab and return
    the open-issue count shown afterwards."""
    page.goto(f"{base_url}/{user}/{repo}/-/issues")
    page.click(f"form[action='/{user}/{repo}/-/issues/new'] button[type=submit]")
    page.wait_for_url(re.compile(r"/-/issues$"))
