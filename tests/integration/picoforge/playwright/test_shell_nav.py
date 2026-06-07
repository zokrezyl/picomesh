"""The shared app shell (topbar + sidebar) and global search, driven through
a real browser.

The happy-path suite walks one journey but never asserts the chrome that
frames every page — the brand link, the global-search box, the sidebar's
Projects/Groups/Issues/Pipelines nav, and the admin-only entry points. A
regression in the shell (a dropped nav item, a renamed `/-/` href, search
breaking) would render every page subtly wrong yet pass the journey test.
These cover the shell itself.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER
from forge import ensure_repo


def test_topbar_and_sidebar_present(page, base_url):
    """A signed-in admin sees the full chrome: brand, global search, the New
    + Admin actions, and the four app-nav items in the sidebar."""
    sign_in_or_register(page, base_url)  # USER — site admin on a fresh stack

    topbar = page.locator("header.topbar")
    expect(topbar.locator("a.brand[href='/-/repos']")).to_be_visible()
    expect(topbar.locator("form.global-search[action='/-/search'] input[name=q]")
           ).to_be_visible()
    # App-space actions the topbar advertises to an admin.
    expect(topbar.locator("a[href='/-/repos/new']")).to_have_count(1)
    expect(topbar.locator("a[href='/-/admin']")).to_have_count(1)

    sidebar = page.locator("aside.sidebar nav.sidebar-nav")
    for href in ("/-/repos", "/-/groups", "/-/dashboard/issues", "/-/dashboard/runs"):
        expect(sidebar.locator(f"a[href='{href}']")).to_have_count(1)


def test_sidebar_navigates_to_groups(page, base_url):
    """Clicking the sidebar Groups item lands on the groups page — proving the
    nav href is live, not just present in the DOM."""
    sign_in_or_register(page, base_url)
    page.click("aside.sidebar nav.sidebar-nav a[href='/-/groups']")
    expect(page).to_have_url(re.compile(r"/-/groups$"))
    expect(page.locator("h1")).to_have_text("Groups")


def test_global_search_finds_repo(page, base_url):
    """The topbar search filters the user's accessible repos by name. Create a
    repo, search a substring of its name, and confirm it is listed and links
    to its tree page."""
    sign_in_or_register(page, base_url)
    repo = "searchable-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/-/search?q=searchable")
    expect(page.locator("h1")).to_have_text("Search")
    expect(page.locator(f"a[href='/{USER}/{repo}/-/tree']")).to_be_visible()


def test_global_search_no_match(page, base_url):
    """A query that matches nothing renders the explicit empty state rather
    than a stale or crashing list."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/search?q=zzz-no-such-repo-zzz")
    expect(page.locator("h1")).to_have_text("Search")
    expect(page.locator("text=No matching repositories.")).to_be_visible()


def test_search_box_submits_to_search_page(page, base_url):
    """Typing in the topbar box and submitting routes to /-/search?q=… — the
    real path a user takes, not a hand-built URL."""
    sign_in_or_register(page, base_url)
    box = page.locator("header.topbar form.global-search input[name=q]")
    box.fill("demo")
    box.press("Enter")
    expect(page).to_have_url(re.compile(r"/-/search\?q=demo$"))
    expect(page.locator("h1")).to_have_text("Search")
