"""The admin area pages beyond Users (which test_mesh_correctness already
covers): Overview, Repositories, Tokens, Services, and Namespaces — plus the
namespace detail page and the admin-only PAT mint relay.

These are the site-admin surfaces. The first registrant on a fresh stack
(USER) is the bootstrap site admin, so the same sign-in makes every page here
reachable. A renamed count/list method, a dropped admin mutation relay, or a
broken /_describe service roster would break one of these without touching the
happy path.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER


def test_admin_overview(page, base_url):
    """The admin landing shows the deployment stat tiles and the Manage table
    linking into each admin page."""
    sign_in_or_register(page, base_url)  # USER — site admin on a fresh stack
    resp = page.goto(f"{base_url}/-/admin")
    assert resp.status == 200, f"/-/admin as admin should be 200, got {resp.status}"
    expect(page.locator("h1")).to_have_text("Admin")

    stats = page.locator("section.stats-grid")
    expect(stats).to_be_visible()
    for label in ("users", "repositories", "active tokens", "services", "pipeline runs"):
        expect(stats.locator(".stat-label", has_text=re.compile(rf"^{label}$"))
               ).to_have_count(1)

    for href in ("/-/admin/users", "/-/admin/repos", "/-/admin/tokens", "/-/admin/services"):
        expect(page.locator(f"table.file-table a[href='{href}']")).to_have_count(1)


def test_admin_repos_count_tile(page, base_url):
    """The admin Repositories page shows a real mesh-wide repository total
    (count_total round-tripped), not an em-dash."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/admin/repos")
    expect(page.locator("h1")).to_have_text("Repositories")
    tile = page.locator("section.stats-grid .stat", has_text="repositories")
    value = tile.locator(".stat-value").inner_text().strip()
    assert value.isdigit(), f"repositories tile should be an integer, got {value!r}"


def test_admin_tokens_mint_relay(page, base_url):
    """The Tokens page renders the mint form, and submitting it as an admin is
    relayed to the gateway and redirects back to the Tokens page (the
    admin-mutation relay path is live for an admin)."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/admin/tokens")
    expect(page.locator("h1")).to_have_text("Tokens")

    form = page.locator("form[action='/-/admin/tokens/mint_pat']")
    expect(form).to_be_visible()
    form.locator("input[name=uid]").fill("100")
    form.locator("button[type=submit]").click()
    page.wait_for_url(re.compile(r"/-/admin/tokens$"))
    expect(page.locator("h1")).to_have_text("Tokens")


def test_admin_services_roster(page, base_url):
    """The Services page lists the live mesh services discovered from the
    gateway /_describe — the core backends must appear, each marked active."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/admin/services")
    expect(page.locator("h1")).to_have_text("Services")

    table = page.locator("table.service-table")
    expect(table).to_be_visible()
    for service in ("git_repo", "accounts", "session"):
        expect(table.locator("code", has_text=re.compile(rf"^{service}$"))
               ).to_have_count(1)
    expect(table.locator("span.badge.succeeded").first).to_be_visible()


def test_admin_namespaces_lists_and_creates(page, base_url):
    """The admin Namespaces page lists every namespace (the personal one for
    USER must be present) and offers the create-group form."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/admin/namespaces")
    expect(page.locator("h1")).to_have_text("Namespaces")

    table = page.locator("table.file-table")
    expect(table).to_be_visible()
    expect(table.locator(f"a[href='/-/admin/namespaces/{USER}']").first).to_be_visible()

    create = page.locator("form[action='/-/admin/namespaces/create']")
    expect(create.locator("input[name=slug]")).to_be_visible()
    expect(create.locator("input[name=parent]")).to_be_visible()


def test_admin_namespace_detail_members(page, base_url):
    """Opening a namespace's manage page shows its Members table (the owner is
    a member) and the grant-a-role form."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/admin/namespaces/{USER}")
    expect(page.locator("h1")).to_have_text(USER)
    # Back link to the namespace index (the sidebar nav shares the href, so
    # match on the link's own text).
    expect(page.locator("main a:has-text('All namespaces')")).to_be_visible()

    members = page.locator("section.panel", has_text="Members").locator("table.file-table")
    expect(members).to_be_visible()
    expect(members).to_contain_text(USER)  # owner is a member

    grant = page.locator("form[action='/-/admin/namespaces/add_member']")
    expect(grant.locator("input[name=username]")).to_be_visible()
    expect(grant.locator("select[name=role]")).to_be_visible()
