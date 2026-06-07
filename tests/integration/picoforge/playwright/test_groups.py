"""Group (namespace) management through the browser: the /-/groups list, the
create-a-group form, and the group detail page's member grant/revoke flow.

test_rbac.py exercises the RBAC model at the RPC layer; this is the operator's
view of the same model — the actual GitLab-style group UI a maintainer uses.
None of it is touched by the happy-path suite, so a broken group page, a
dropped ns_create / ns_add_member relay, or a mis-wired members table would go
unnoticed.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, register, login, PASSWORD, USER
from forge import ensure_group


def test_groups_page_lists_personal_namespace(page, base_url):
    """Every user belongs to their own personal namespace as owner; the Groups
    page lists it, plus the create-a-group and manage-by-path forms."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/groups")
    expect(page.locator("h1")).to_have_text("Groups")

    table = page.locator("table.file-table")
    expect(table).to_be_visible()
    # The personal namespace row (named after the user) with the owner role.
    row = table.locator("tr", has=page.locator(f"a[href='/{USER}']"))
    expect(row).to_contain_text("owner")

    # The two management forms are present.
    expect(page.locator("form[action='/-/groups/create'] input[name=slug]")).to_be_visible()
    expect(page.locator("form[action='/-/groups/go'] input[name=path]")).to_be_visible()


def test_create_group_appears_with_owner_role(page, base_url):
    """Creating a top-level group via the form persists an owner membership for
    the creator. The /-/groups list is sourced from the session's namespace
    claims, which are snapshotted into the access token at login — so the new
    group surfaces after re-authenticating, listed with the owner role and a
    manage link. (A fresh login picking it up proves the membership was really
    persisted, not just echoed by the request that created it.)"""
    sign_in_or_register(page, base_url)
    group = "uitestgrp"
    ensure_group(page, base_url, group)

    # Refresh the session so its namespace claims include the new group.
    login(page, base_url, USER, PASSWORD)
    page.goto(f"{base_url}/-/groups")
    row = page.locator("table.file-table tr", has=page.locator(f"a[href='/{group}']"))
    expect(row).to_contain_text("owner")
    # Owner gets the maintainer-only manage link to the group detail page.
    expect(row.locator(f"a[href='/-/groups/{group}']")).to_be_visible()


def test_group_detail_grant_and_revoke_member(page, base_url):
    """The group owner can grant another registered user a role through the
    detail page's form, see them in the Members table, then revoke them. This
    is the full member-management round-trip via the UI."""
    sign_in_or_register(page, base_url)
    group = "uitestmgrp"
    ensure_group(page, base_url, group)

    # A second account to grant a role to. Registering signs that user in, so
    # sign back in as the group owner afterwards.
    grantee = "grantee"
    register(page, base_url, grantee, PASSWORD)
    sign_in_or_register(page, base_url)  # back to USER (group owner)

    # Open the group's management page.
    page.goto(f"{base_url}/-/groups/{group}")
    expect(page.locator("h1")).to_have_text(group)
    members = page.locator("section.panel", has_text="Members").locator("table.file-table")
    expect(members).to_be_visible()

    # Grant `grantee` the developer role via the form.
    grant = page.locator("form[action='/-/groups/add_member']")
    grant.locator("input[name=username]").fill(grantee)
    grant.locator("select[name=role]").select_option("developer")
    grant.locator("button[type=submit]").click()
    page.wait_for_load_state("networkidle")

    members = page.locator("section.panel", has_text="Members").locator("table.file-table")
    grantee_row = members.locator("tr", has_text=grantee)
    expect(grantee_row).to_contain_text("developer")

    # Revoke the membership again; the row disappears from the table.
    grantee_row.locator("form[action='/-/groups/remove_member'] button[type=submit]").click()
    page.wait_for_load_state("networkidle")
    members = page.locator("section.panel", has_text="Members").locator("table.file-table")
    expect(members.locator("tr", has_text=grantee)).to_have_count(0)


def test_groups_go_jumps_to_namespace(page, base_url):
    """The 'Manage a namespace by path' box jumps straight to a group's detail
    page — the path a maintainer of an inherited subgroup uses."""
    sign_in_or_register(page, base_url)
    group = "uitestjump"
    ensure_group(page, base_url, group)

    page.fill("form[action='/-/groups/go'] input[name=path]", group)
    page.click("form[action='/-/groups/go'] button[type=submit]")
    expect(page).to_have_url(re.compile(rf"/-/groups/{group}$"))
    expect(page.locator("h1")).to_have_text(group)
