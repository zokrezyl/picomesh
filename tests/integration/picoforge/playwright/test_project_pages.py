"""Project-scoped pages the happy path doesn't open: the repo Settings tab,
the bare-path namespace landing, and the Issues/Pipelines action forms
(close-by-id, lease-a-job) that complete the project shell.

The journey test creates files and files an issue, but never visits Settings,
never loads a namespace landing page, and never asserts the close/lease action
forms exist. Those are the GitLab-style project surfaces a regression in the
`/-/<verb>` route table or the namespace parser would break.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER
from forge import ensure_repo


def test_repo_settings_page(page, base_url):
    """The Settings tab shows the project metadata (owner, default branch,
    visibility) and keeps the project sub-nav with Settings marked active."""
    sign_in_or_register(page, base_url)
    repo = "settings-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}/{repo}/-/settings")
    expect(page.locator("nav.project-tabs")).to_be_visible()
    expect(page.locator("nav.project-tabs a.active")).to_have_text("Settings")

    grid = page.locator("table.grid")
    expect(grid).to_contain_text(USER)      # Owner
    expect(grid).to_contain_text("main")    # Default branch
    expect(grid).to_contain_text("public")  # Visibility


def test_namespace_landing_lists_repos(page, base_url):
    """A bare path `/<namespace>` is the account landing — it lists the
    namespace's repos. The page header is the namespace name and the ensured
    repo links to its tree."""
    sign_in_or_register(page, base_url)
    repo = "landing-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}")
    expect(page.locator("h1")).to_have_text(USER)
    expect(page.locator(f"a[href='/{USER}/{repo}/-/tree']").first).to_be_visible()


def test_issues_page_has_close_form(page, base_url):
    """The project Issues tab renders the New-issue action and the
    close-by-id form — the final shape of the issue work queue."""
    sign_in_or_register(page, base_url)
    repo = "issue-forms-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}/{repo}/-/issues")
    expect(page.locator(f"form[action='/{USER}/{repo}/-/issues/new'] button.primary")
           ).to_be_visible()
    close_form = page.locator(f"form[action='/{USER}/{repo}/-/issues/close']")
    expect(close_form).to_be_visible()
    expect(close_form.locator("input[name=issue_id]")).to_be_visible()


def test_issue_count_increments_then_close_form_round_trips(page, base_url):
    """Filing an issue bumps the open count, and submitting the close form
    redirects back to the Issues tab (the close action path is wired, not a
    404). We use the count the New action produced as the id to close."""
    sign_in_or_register(page, base_url)
    repo = "issue-close-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}/{repo}/-/issues")
    page.click(f"form[action='/{USER}/{repo}/-/issues/new'] button[type=submit]")
    page.wait_for_url(re.compile(r"/-/issues$"))
    expect(page.locator("text=/[1-9][0-9]* open issue/")).to_be_visible()

    # Close issue #1 (the first one filed on this fresh repo). The form posts
    # to /…/issues/close and 303s back to the Issues tab regardless of whether
    # the id existed — what we assert is that the action route is live.
    page.fill(f"form[action='/{USER}/{repo}/-/issues/close'] input[name=issue_id]", "1")
    page.click(f"form[action='/{USER}/{repo}/-/issues/close'] button[type=submit]")
    page.wait_for_url(re.compile(r"/-/issues$"))
    # Landed back on the Issues tab (the close action route is live, not a 404).
    expect(page.locator("nav.project-tabs")).to_be_visible()


def test_runs_page_has_lease_form(page, base_url):
    """The project Pipelines tab renders the Run-pipeline action and the
    runner lease form."""
    sign_in_or_register(page, base_url)
    repo = "run-forms-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}/{repo}/-/runs")
    expect(page.locator("button.primary:has-text('Run pipeline')")).to_be_visible()
    lease_form = page.locator(f"form[action='/{USER}/{repo}/runs/lease']")
    expect(lease_form).to_be_visible()
    expect(lease_form.locator("input[name=runner]")).to_be_visible()
