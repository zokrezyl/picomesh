"""Cross-page consistency: an action taken on a project's tab is reflected on
the mesh-wide dashboard. These tie two pages together through the real
backends, catching the class of bug where a write succeeds but the aggregate
read (a different RPC path) doesn't see it.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER
from forge import ensure_repo, file_an_issue


def _queued_count(page, base_url):
    """The current global 'queued' pipeline count from the runs dashboard."""
    page.goto(f"{base_url}/-/dashboard/runs")
    row = page.locator("table.pipeline-table tr", has_text="queued")
    return int(row.locator("td").last.inner_text().strip())


def test_filed_issue_shows_on_issues_dashboard(page, base_url):
    """Filing an issue on a repo bumps that repo's open count on the issues
    dashboard — the per-repo aggregate read sees the write."""
    sign_in_or_register(page, base_url)
    repo = "dash-consistency-issues"
    ensure_repo(page, base_url, USER, repo)
    file_an_issue(page, base_url, USER, repo)

    page.goto(f"{base_url}/-/dashboard/issues")
    row = page.locator("table.file-table tr",
                       has=page.locator(f"a[href='/{USER}/{repo}/-/issues']"))
    badge = row.locator("span.badge.open")
    expect(badge).to_be_visible()
    assert int(badge.inner_text().strip()) >= 1, \
        "filed issue should raise the repo's open count on the dashboard"


def test_enqueue_run_increments_runs_dashboard(page, base_url):
    """Enqueuing a pipeline run on a repo increases the global queued count on
    the runs dashboard (the counter is mesh-wide; no test leases, so it only
    grows)."""
    sign_in_or_register(page, base_url)
    repo = "dash-consistency-runs"
    ensure_repo(page, base_url, USER, repo)

    before = _queued_count(page, base_url)

    page.goto(f"{base_url}/{USER}/{repo}/-/runs")
    page.click("button.primary:has-text('Run pipeline')")
    page.wait_for_url(re.compile(r"/-/runs$"))

    after = _queued_count(page, base_url)
    assert after >= before + 1, \
        f"queued count should rise after enqueue (before={before}, after={after})"
