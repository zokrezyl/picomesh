"""Namespace RBAC as seen through the browser, with two real concurrent
sessions. test_rbac.py proves the model at the RPC layer; this proves the
webapp honours it: a second user cannot see another user's repos, search is
scoped to what the caller can read, a non-maintainer is forbidden from a
group's management page, and the error/grant surfaces behave.

Each test signs the site owner (USER) in first so the *second* account is
guaranteed non-admin, then drives that second user from an independent browser
context (its own cookie jar).

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER
from forge import ensure_repo, ensure_group, open_second_user


def test_repos_list_is_isolated_per_user(page, base_url):
    """A repo owned by USER must not appear in a different user's projects
    list — discovery is role-based, so a non-member sees nothing of it."""
    sign_in_or_register(page, base_url)  # USER, site owner
    repo = "private-to-owner"
    ensure_repo(page, base_url, USER, repo)

    bob = open_second_user(page, base_url, "bob_iso", "hunter2")
    try:
        bob.goto(f"{base_url}/-/repos")
        expect(bob.locator("h1")).to_have_text("Repositories")
        assert bob.locator(f"a[href='/{USER}/{repo}/-/tree']").count() == 0, \
            "another user's repo must not show in a non-member's projects list"
    finally:
        bob.context.close()


def test_search_is_scoped_to_caller(page, base_url):
    """Global search filters the CALLER's accessible repos. A second user
    searching for USER's repo name finds nothing."""
    sign_in_or_register(page, base_url)
    repo = "owner-secret-repo"
    ensure_repo(page, base_url, USER, repo)

    bob = open_second_user(page, base_url, "bob_search", "hunter2")
    try:
        bob.goto(f"{base_url}/-/search?q=owner-secret")
        expect(bob.locator("h1")).to_have_text("Search")
        expect(bob.locator("text=No matching repositories.")).to_be_visible()
        assert bob.locator(f"a[href='/{USER}/{repo}/-/tree']").count() == 0
    finally:
        bob.context.close()


def test_non_maintainer_forbidden_from_group_detail(page, base_url):
    """A user who is not a maintainer of a group must get 403 from its
    management page — not the member list."""
    sign_in_or_register(page, base_url)
    group = "ownersgrp"
    ensure_group(page, base_url, group)

    bob = open_second_user(page, base_url, "bob_grp", "hunter2")
    try:
        resp = bob.goto(f"{base_url}/-/groups/{group}")
        assert resp.status == 403, f"non-maintainer group detail should be 403, got {resp.status}"
        expect(bob.locator("h1")).to_have_text("Forbidden")
    finally:
        bob.context.close()


def test_grant_unknown_user_surfaces_error(page, base_url):
    """Granting a role to a username that does not exist is rejected by the
    backend and surfaced as an Action-failed page, not a silent no-op."""
    sign_in_or_register(page, base_url)
    group = "granterrgrp"
    ensure_group(page, base_url, group)

    page.goto(f"{base_url}/-/groups/{group}")
    grant = page.locator("form[action='/-/groups/add_member']")
    grant.locator("input[name=username]").fill("nobody-such-user-xyz")
    grant.locator("select[name=role]").select_option("developer")
    grant.locator("button[type=submit]").click()
    page.wait_for_load_state("networkidle")
    expect(page.locator("h1")).to_have_text("Action failed")
    # The error panel's Back link lives in <main> (the admin sidebar also has a
    # "Back to app" foot link, so scope to the content area).
    expect(page.locator("main a:has-text('Back')")).to_be_visible()


def test_create_subgroup_appears_in_admin_namespaces(page, base_url):
    """Creating a subgroup from a group's detail page nests it under the
    parent; the admin namespace index then lists '<parent>/<child>'."""
    sign_in_or_register(page, base_url)  # USER is admin → can see the index
    parent = "subparentgrp"
    ensure_group(page, base_url, parent)

    page.goto(f"{base_url}/-/groups/{parent}")
    sub = page.locator("form[action='/-/groups/create']")
    sub.locator("input[name=slug]").fill("backend")
    sub.locator("button[type=submit]").click()
    page.wait_for_load_state("networkidle")

    # The subgroup is a real namespace now: the admin index lists it.
    page.goto(f"{base_url}/-/admin/namespaces")
    expect(page.locator(f"a[href='/-/admin/namespaces/{parent}/backend']").first
           ).to_be_visible()
