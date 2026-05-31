"""End-to-end UI flow for the picoforge webapp, driven through a real
browser (Playwright + system Chrome).

This clicks through the app exactly like a user: sign up, create a
repository, create a file in the Monaco editor, browse it, edit it, file an
issue, enqueue a pipeline run, and sign out — asserting the rendered UI at
each step. All data round-trips browser -> picoforge-webapp -> gateway
/_rpc -> backends; nothing here pokes a backend port directly.
"""

import re

import pytest
from playwright.sync_api import expect

USER = "uitester"
PASSWORD = "hunter2"
REPO = "demo-site"
FILE_PATH = "README.md"
FILE_V1 = "# Demo site\n\nhello from the integration test\n"
EDIT_MARKER = "edited by the integration test"


# ---- helpers --------------------------------------------------------------

def _signed_in(page) -> bool:
    """True when the top nav shows the signed-in controls."""
    return page.locator("nav.topnav .nav-links").count() > 0


def sign_in_or_register(page, base_url):
    """Register USER; if the account already exists, sign in instead.
    Leaves the browser on the /repos page, signed in."""
    page.goto(f"{base_url}/register")
    page.fill("input[name=username]", USER)
    page.fill("input[name=password]", PASSWORD)
    page.click("button[type=submit]")
    page.wait_for_load_state("networkidle")

    if not _signed_in(page):
        # Duplicate account (re-run against a persistent stack) -> sign in.
        page.goto(f"{base_url}/login")
        page.fill("input[name=username]", USER)
        page.fill("input[name=password]", PASSWORD)
        page.click("button[type=submit]")
        page.wait_for_load_state("networkidle")

    expect(page).to_have_url(re.compile(r"/repos$"))
    expect(page.locator("h1")).to_have_text("Repositories")
    assert _signed_in(page), "top nav should show signed-in controls after auth"


def monaco_focus(page):
    """Wait for the Monaco editor to render and focus its text area, the
    same way a user clicks into the editor before typing."""
    page.wait_for_selector("#editor .monaco-editor", timeout=20000)
    page.wait_for_selector("#editor textarea.inputarea", timeout=20000)
    page.click("#editor .monaco-editor .view-lines")


def monaco_type(page, text):
    """Type `text` into the editor with real keystrokes (no JS injection)."""
    monaco_focus(page)
    page.keyboard.type(text)


def monaco_text(page):
    """Read what the editor is actually showing the user (rendered lines).
    Monaco renders spaces as non-breaking spaces in the DOM, so normalize
    them back so substring assertions match the typed text."""
    page.wait_for_selector("#editor .view-lines", timeout=20000)
    return page.locator("#editor .view-lines").inner_text().replace("\xa0", " ")


# ---- the flow -------------------------------------------------------------

def test_full_ui_flow(page, base_url):
    # 1. Sign up (or in) through the form.
    sign_in_or_register(page, base_url)

    # 2. Create a repository via the /repos form.
    page.fill("#new input[name=name]", REPO)
    page.click("#new button[type=submit]")
    page.wait_for_load_state("networkidle")

    # 3. The new repo shows up in the list, linking to /<user>/<repo>.
    repo_link = page.locator(f"a[href='/{USER}/{REPO}']").first
    expect(repo_link).to_be_visible()

    # 4. Open the repository browser.
    repo_link.click()
    page.wait_for_load_state("networkidle")
    expect(page).to_have_url(re.compile(rf"/{USER}/{REPO}$"))
    # The repo sub-nav (Code / Issues / Pipelines) is present.
    expect(page.locator("nav.repo-tabs")).to_be_visible()

    # 5. Create a file: click "New file", type the path, TYPE the content
    #    into the Monaco editor with real keystrokes, then click Commit.
    page.click("a.btn:has-text('New file')")
    page.wait_for_url(re.compile(r"/new$"))
    page.fill("input[name=path]", FILE_PATH)
    monaco_type(page, FILE_V1)
    page.click("#f button[type=submit]")  # Commit (the editor form, not the nav sign-out)
    page.wait_for_load_state("networkidle")

    # 6. The file is now listed in the repo tree, linking to its editor.
    file_link = page.locator(f"a[href*='/edit?path={FILE_PATH}']").first
    expect(file_link).to_be_visible()

    # 7. Click the file open — the editor shows the committed content.
    file_link.click()
    page.wait_for_url(re.compile(r"/edit\?path="))
    assert "hello from the integration test" in monaco_text(page)

    # 8. Edit the file: jump to the end and TYPE a marker line, then Commit.
    monaco_focus(page)
    page.keyboard.press("Control+End")
    page.keyboard.type("\n" + EDIT_MARKER + "\n")
    page.click("#f button[type=submit]")
    page.wait_for_load_state("networkidle")

    # 9. Re-open the file by clicking it again — the typed edit persisted.
    page.locator(f"a[href*='/edit?path={FILE_PATH}']").first.click()
    page.wait_for_url(re.compile(r"/edit\?path="))
    assert EDIT_MARKER in monaco_text(page), "edit did not round-trip via git_repo"

    # Back to the repo via the editor's "files" link, so we can use the tabs.
    page.click("a.btn:has-text('files')")
    page.wait_for_url(re.compile(rf"/{USER}/{REPO}$"))

    # 10. Issues: navigate via the repo tab, file one by clicking the button.
    page.click("nav.repo-tabs a:has-text('Issues')")
    page.wait_for_url(re.compile(r"/issues$"))
    expect(page.locator("h1")).to_have_text("Issues")
    page.click("form[action$='/issues/new'] button[type=submit]")
    page.wait_for_load_state("networkidle")
    expect(page.locator("text=/[1-9][0-9]* open issue/")).to_be_visible()

    # 11. Pipelines: navigate via the repo tab, enqueue by clicking.
    page.click("nav.repo-tabs a:has-text('Pipelines')")
    page.wait_for_url(re.compile(r"/runs$"))
    expect(page.locator("h1")).to_have_text("Pipeline runs")
    page.click("form[action$='/runs/new'] button[type=submit]")
    page.wait_for_load_state("networkidle")
    queued = page.locator("table.grid tr", has_text="queued").locator("span.badge")
    assert int(queued.inner_text().strip()) >= 1, "queued run count should be >= 1"

    # 12. Sign out by clicking the nav button — back to the sign-in page.
    page.click("nav.topnav form[action='/logout'] button[type=submit]")
    page.wait_for_url(re.compile(r"/login$"))
    expect(page.locator("h1")).to_have_text("Sign in")


def test_gateway_serves_no_html(base_url):
    """The webapp owns pages; the gateway must 404 HTML GETs. Cross-checks
    the B1/gh#5 invariant from the browser's side, via urllib."""
    import urllib.error
    import urllib.request

    # Derive the gateway URL: webapp is :8081, gateway :8080 on the same host.
    host = base_url.rsplit(":", 1)[0]
    gateway = f"{host}:8080"
    for path in ("/", "/login", "/repos"):
        try:
            code = urllib.request.urlopen(gateway + path, timeout=8).getcode()
        except urllib.error.HTTPError as e:
            code = e.code
        assert code == 404, f"gateway {path} returned {code}, expected 404 (API-only)"
