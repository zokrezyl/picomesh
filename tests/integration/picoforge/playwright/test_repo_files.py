"""Repository file-tree navigation in the browser: the empty-repo state,
creating a file in a subdirectory, descending into and back out of a
directory, and the editor's breadcrumb / read-only path behaviour.

The happy path creates and edits one top-level file. It never exercises the
tree's directory links, the parent ('..') link, the New-file-in-a-subdir
button (which must carry ?dir=), or the empty-repo prompt. Those are the
git_repo.read_tree-backed surfaces a path-handling or routing regression would
break.

All data round-trips browser -> picoforge-webapp -> gateway /_rpc -> backends.
"""

import re

from playwright.sync_api import expect

from helpers import sign_in_or_register, USER

# Reuse the Monaco helpers from the top-level flow test (real keystrokes, no
# JS injection) so this suite drives the editor exactly as a user does.
from test_ui_flow import monaco_type, monaco_text, monaco_focus
from forge import ensure_repo


def _commit_new_file(page, base_url, user, repo, path, content, dir_=None):
    """Open the New-file editor (optionally seeded with ?dir=), type a path +
    content, and Commit. Leaves the browser on the resulting tree page."""
    if dir_:
        page.goto(f"{base_url}/{user}/{repo}/-/new?dir={dir_}")
    else:
        page.goto(f"{base_url}/{user}/{repo}/-/new")
    page.fill("input[name=path]", path)
    monaco_type(page, content)
    page.click("#f button[type=submit]")
    page.wait_for_load_state("networkidle")


def test_empty_repo_shows_empty_state(page, base_url):
    """A freshly created repo has no files — the tree shows the empty-repo
    prompt and the New-file action, not an error."""
    sign_in_or_register(page, base_url)
    repo = "empty-repo-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}/{repo}/-/tree")
    expect(page.locator("nav.project-tabs")).to_be_visible()
    expect(page.locator("text=Empty repository.")).to_be_visible()
    # Two "New file" links exist (the project header and the file panel); either
    # proving the action is offered is enough here.
    expect(page.locator("a.btn:has-text('New file')").first).to_be_visible()


def test_create_file_in_subdirectory(page, base_url):
    """Committing docs/guide.md creates the directory in the tree; descending
    into it shows the file (linking to its editor) and a parent '..' link;
    opening the file round-trips its content."""
    sign_in_or_register(page, base_url)
    repo = "subdir-demo"
    ensure_repo(page, base_url, USER, repo)

    body = "# Guide\n\nnested file from the integration test\n"
    _commit_new_file(page, base_url, USER, repo, "docs/guide.md", body)

    # The top-level tree now lists the `docs` directory, linking deeper.
    page.goto(f"{base_url}/{USER}/{repo}/-/tree")
    docs_link = page.locator(f"a.dir[href='/{USER}/{repo}/-/tree?dir=docs']")
    expect(docs_link).to_be_visible()

    # Descend into docs/: the file links to its editor and a '..' link exists.
    docs_link.click()
    page.wait_for_url(re.compile(r"\?dir=docs$"))
    expect(page.locator(f"a[href='/{USER}/{repo}/-/edit?path=docs/guide.md']")
           ).to_be_visible()
    expect(page.locator("a.dir", has_text="..")).to_be_visible()
    # The file-panel's New-file action inside a subdir carries the dir so the
    # new file lands in docs/, not the root. (The project-header New-file link
    # is the bare one; the panel button — class "btn small" — is dir-scoped.)
    new_in_dir = page.locator("a.btn.small:has-text('New file')")
    expect(new_in_dir).to_have_attribute("href", re.compile(r"\?dir=docs$"))

    # Open the file — its content round-tripped through git_repo.
    page.locator(f"a[href='/{USER}/{repo}/-/edit?path=docs/guide.md']").click()
    page.wait_for_url(re.compile(r"/-/edit\?path=docs/guide.md$"))
    assert "nested file from the integration test" in monaco_text(page)


def test_editor_path_is_readonly_when_editing(page, base_url):
    """Opening an existing file shows the editor with the path field locked
    (you edit content, not the name) and a breadcrumb back to the repo tree."""
    sign_in_or_register(page, base_url)
    repo = "readonly-path-demo"
    ensure_repo(page, base_url, USER, repo)
    _commit_new_file(page, base_url, USER, repo, "NOTES.md", "first\n")

    page.goto(f"{base_url}/{USER}/{repo}/-/edit?path=NOTES.md")
    expect(page.locator("h1")).to_have_text("Edit file")
    path_field = page.locator("form#f input[name=path]")
    expect(path_field).to_have_attribute("readonly", re.compile(r".*"))
    expect(path_field).to_have_value("NOTES.md")
    # Breadcrumb links back to the repo tree.
    expect(page.locator(f"header.editor-header a[href='/{USER}/{repo}/-/tree']").first
           ).to_be_visible()


def test_new_file_editor_is_blank_with_editable_path(page, base_url):
    """The New-file editor has an empty, editable path field and the New-file
    heading — distinct from the read-only edit view."""
    sign_in_or_register(page, base_url)
    repo = "newfile-demo"
    ensure_repo(page, base_url, USER, repo)

    page.goto(f"{base_url}/{USER}/{repo}/-/new")
    expect(page.locator("h1")).to_have_text("New file")
    path_field = page.locator("form#f input[name=path]")
    expect(path_field).to_have_value("")
    assert path_field.get_attribute("readonly") is None, \
        "the New-file path field must be editable, not readonly"
    # The Monaco editor mounts and is focusable.
    monaco_focus(page)
