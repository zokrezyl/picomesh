"""The two cross-repo dashboards: /-/dashboard/issues and /-/dashboard/runs.

These aggregate state across every repo the signed-in user can access. The
happy-path suite only ever opens a single project's Issues/Pipelines tabs, so
the mesh-wide dashboards — and the RBAC repo-discovery that backs them — have
no browser coverage. A renamed aggregate count method, or the discovery
returning nothing, would silently empty these pages.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER
from forge import ensure_repo


def test_dashboard_issues_lists_repositories(page, base_url):
    """The issues dashboard lists the user's repos with a per-repo open count.
    With at least one owned repo present, the by-repository table renders and
    links the repo to its Issues tab."""
    sign_in_or_register(page, base_url)
    repo = "dash-issues-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/-/dashboard/issues")
    expect(page.locator("h1")).to_have_text("Issues")
    expect(page.locator("text=Open issues across your repositories.")).to_be_visible()
    # The repo shows up in the by-repository table, linking to its Issues tab.
    expect(page.locator(f"a[href='/{USER}/{repo}/-/issues']").first).to_be_visible()


def test_dashboard_runs_shows_state_counts(page, base_url):
    """The pipelines dashboard renders the queued/running/finished summary
    table sourced from git_pipeline's global counters."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/dashboard/runs")
    expect(page.locator("h1")).to_have_text("Pipelines")

    table = page.locator("table.pipeline-table")
    expect(table).to_be_visible()
    # The three lifecycle states each render a badge row.
    expect(table.locator("span.badge.queued")).to_have_count(1)
    expect(table.locator("span.badge.running")).to_have_count(1)
    expect(table.locator("span.badge.succeeded")).to_have_count(1)


def test_dashboard_runs_count_is_numeric(page, base_url):
    """Each dashboard row carries a real integer count (the RPC round-tripped),
    never an em-dash placeholder — a dead counter would show '-' instead."""
    sign_in_or_register(page, base_url)
    page.goto(f"{base_url}/-/dashboard/runs")
    queued_row = page.locator("table.pipeline-table tr", has_text="queued")
    count_cell = queued_row.locator("td").last.inner_text().strip()
    assert count_cell.isdigit(), f"queued count should be an integer, got {count_cell!r}"
