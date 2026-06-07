"""Coverage fill-ins for surfaces the other suites skip: the root redirect,
the create-repo form shape, static asset serving, the admin-side namespace
member management POSTs (distinct from the /-/groups ones), and a non-member's
repo-create into a foreign namespace being denied through the UI.

All data round-trips browser/urllib -> picoforge-webapp -> gateway /_rpc ->
backends; nothing pokes a backend port directly.
"""

import re
import urllib.error
import urllib.request

from playwright.sync_api import expect

from helpers import sign_in_or_register, register, USER, PASSWORD
from forge import ensure_repo, open_second_user


def test_root_redirects_to_repos(page, base_url):
    """GET / for a signed-in user lands on the projects page."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/")
    expect(page).to_have_url(re.compile(r"/-/repos$"))
    expect(page.locator("h1")).to_have_text("Repositories")


def test_repos_new_form_shape(page, base_url):
    """The create-repository form offers a namespace field (defaulting to the
    user's personal namespace) with a suggestions datalist, a name field, and
    the create button."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/repos/new")
    expect(page.locator("h1")).to_have_text("New repository")
    form = page.locator("form[action='/-/repos/new']")
    ns = form.locator("input[name=namespace]")
    expect(ns).to_have_value(USER)                         # defaults to personal ns
    expect(ns).to_have_attribute("list", "ns-options")     # datalist wired
    expect(page.locator("datalist#ns-options")).to_have_count(1)
    expect(form.locator("input[name=name]")).to_be_visible()
    expect(form.locator("button[type=submit]")).to_have_text("Create repository")


def test_static_asset_served(base_url):
    """The webapp sidecar serves its own static assets (the gateway must not —
    that invariant is checked elsewhere; here we confirm the webapp does)."""
    with urllib.request.urlopen(f"{base_url}/static/style.css", timeout=10) as resp:
        assert resp.getcode() == 200
        ctype = resp.headers.get("Content-Type", "")
    assert "css" in ctype, f"style.css should be served as CSS, got {ctype!r}"


def test_static_missing_asset_404(base_url):
    """A missing static path 404s rather than leaking the filesystem."""
    try:
        code = urllib.request.urlopen(
            f"{base_url}/static/no-such-file.xyz", timeout=10).getcode()
    except urllib.error.HTTPError as e:
        code = e.code
    assert code == 404, f"missing static asset should 404, got {code}"


def test_admin_namespace_member_management(page, base_url):
    """The admin namespace surface (distinct code path from /-/groups) can
    create a root group, grant a registered user a role, show them in the
    members table, and revoke them."""
    sign_in_or_register(page, base_url)  # USER, site admin

    # A registered user to grant. Registering signs them in, so sign back in as
    # the admin afterwards.
    member = "nsmember"
    register(page, base_url, member, PASSWORD)
    sign_in_or_register(page, base_url)  # back to USER

    # Create a root group from the admin namespaces form (parent left empty).
    group = "adminnsgrp"
    page.goto(f"{base_url}/-/admin/namespaces")
    if page.locator(f"a[href='/-/admin/namespaces/{group}']").count() == 0:
        create = page.locator("form[action='/-/admin/namespaces/create']")
        slug = create.locator("input[name=slug]")
        slug.fill(group)
        # Submit via Enter: the create form sits at the foot of a namespace list
        # that grows as the session runs, and the sticky app-footer can overlap
        # its submit button — pressing Enter avoids the obscured-click flake.
        slug.press("Enter")
        page.wait_for_load_state("networkidle")

    # Grant `member` the developer role via the admin namespace detail page.
    page.goto(f"{base_url}/-/admin/namespaces/{group}")
    expect(page.locator("h1")).to_have_text(group)
    grant = page.locator("form[action='/-/admin/namespaces/add_member']")
    grant.locator("input[name=username]").fill(member)
    grant.locator("select[name=role]").select_option("developer")
    grant.locator("button[type=submit]").click()
    page.wait_for_load_state("networkidle")

    members = page.locator("section.panel", has_text="Members").locator("table.file-table")
    member_row = members.locator("tr", has_text=member)
    expect(member_row).to_contain_text("developer")

    # Revoke; the row disappears.
    member_row.locator(
        "form[action='/-/admin/namespaces/remove_member'] button[type=submit]").click()
    page.wait_for_load_state("networkidle")
    members = page.locator("section.panel", has_text="Members").locator("table.file-table")
    expect(members.locator("tr", has_text=member)).to_have_count(0)


def test_non_member_cannot_create_repo_in_foreign_namespace(page, base_url):
    """Creating a repo in a namespace the caller has no push role on is denied
    by the backend and surfaced as an Action-failed page — authz on the create
    path, driven through the UI."""
    sign_in_or_register(page, base_url)  # USER owns the `uitester` namespace

    bob = open_second_user(page, base_url, "bob_create", "hunter2")
    try:
        bob.goto(f"{base_url}/-/repos/new")
        # Aim the new repo at USER's personal namespace, which bob has no role on.
        bob.fill("input[name=namespace]", USER)
        bob.fill("input[name=name]", "sneaky-repo")
        bob.click("form[action='/-/repos/new'] button[type=submit]")
        bob.wait_for_load_state("networkidle")
        expect(bob.locator("h1")).to_have_text("Action failed")
        # And it really was not created under USER's namespace.
        page.goto(f"{base_url}/{USER}")
        assert page.locator("a[href='/uitester/sneaky-repo/-/tree']").count() == 0, \
            "a repo must not be created in a namespace the caller cannot push to"
    finally:
        bob.context.close()
