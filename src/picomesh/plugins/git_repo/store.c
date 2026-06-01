/* git_repo — repository metadata + on-disk bare repos via libgit2.
 *
 *   make(owner_id, owner_name, repo_name) → repo_id (libgit2 init_bare on disk)
 *   delete(repo_id)                       → 1 / 0   (rm -rf the bare repo)
 *   owner_of(repo_id)                     → owner_uid (0 if missing)
 *   count_for_owner(owner_id)             → number of repos owned
 *   count_total                           → grand total
 *
 * On-disk layout (GitHub-style, per-user parent):
 *
 *   <repos_dir>/<owner_name>/<repo_name>.git
 *
 * `repos_dir` comes from yconfig (`git_repo.repos_dir`, default
 * `/tmp/picoforge/repos`). `owner_name` and `repo_name` are validated
 * against a strict charset (`[A-Za-z0-9._-]`, 1..63 chars) before
 * they go anywhere near the filesystem — preventing both path
 * traversal and SQL-like escape into other names.
 *
 * Metadata is still the in-memory table for now — moving that to the
 * storage plugin is a separate piece of work.
 *
 * libgit2 runtime: `git_libgit2_init()` is reference-counted internally
 * but we never shut it down — process-lifetime. The lazy-init pattern
 * (function-local static "tried" flag) mirrors backend_mdbx's shared
 * env init: there is no file-scope mutable state. */

#define _XOPEN_SOURCE 700  /* nftw / FTW_DEPTH / FTW_PHYS */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yclass/yheaders.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/yloop/yloop.h>

#include <git2.h>

#include <errno.h>
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Metadata slots for live repos. The table is scanned linearly per op, so
 * this balances headroom (a load test creates many repos) against scan cost;
 * a few thousand entries cost only microseconds per op — far below a network
 * round-trip. */
#define REPOS_MAX     4096
#define REPO_NAME_MAX 64

struct repo_entry {
    uint32_t repo_id;
    uint32_t owner_id;
    /* Names kept on the metadata row so store_delete can locate the
     * on-disk directory without taking new args. The frontend used to
     * pass these in for make and discard them; storing them once at
     * make time keeps the wire surface small. */
    char     owner_name[REPO_NAME_MAX];
    char     repo_name[REPO_NAME_MAX];
    /* Visibility, GitHub/GitLab-style. 0 = private (only the owner may
     * read contents), non-zero = public (anyone, incl. anonymous, may
     * read). Writes are owner-only regardless. New repos default private. */
    int is_public;
    int used;
};

struct PICOMESH_CLASS_ANNOTATE("class@git_repo:store") git_repo_store_data {
    struct repo_entry entries[REPOS_MAX];
    size_t count;
    uint32_t next_id;
};

static struct git_repo_store_data *gr(struct object *obj)
{
    return (struct git_repo_store_data *)((char *)obj + sizeof(struct object));
}

/* Resolve `git_repo.repos_dir` from yconfig; default is a per-host tmp
 * tree. The pointer is owned by yconfig (stable for the process life). */
static const char *resolve_repos_dir(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (e) {
        struct yconfig_node_ptr_result r =
            yconfig_get(picomesh_engine_config(e), "git_repo.repos_dir");
        if (PICOMESH_IS_OK(r) && r.value) {
            const char *s = yconfig_node_as_string(r.value, NULL);
            if (s && *s) return s;
        }
    }
    return "/tmp/picoforge/repos";
}

/* Whether to create the on-disk bare repo (libgit2 git_repository_init)
 * at make time. Default ON. Set `git_repo.disk_init: false` to record
 * only the metadata — the bare repo is needed for real git push/clone,
 * but NOT for the HTML UI (listing/browsing), and `git_repository_init`
 * does dozens of tiny file writes that are crippling slow on the
 * in-browser wasm-emulated disk (it hangs create-repo). The demo turns
 * it off; real deployments leave it on. */
static int repo_disk_init_enabled(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (e) {
        struct yconfig_node_ptr_result r =
            yconfig_get(picomesh_engine_config(e), "git_repo.disk_init");
        if (PICOMESH_IS_OK(r) && r.value)
            return yconfig_node_as_int(r.value, 1) != 0;
    }
    return 1;
}

/* Initialise libgit2 once for the process. Subsequent callers reuse
 * the same global state libgit2 manages internally. The function-local
 * `tried` flag prevents the call from re-running on failure. */
static int ensure_libgit2(void)
{
    static int tried = 0;
    static int ok = 0;
    if (tried) return ok;
    int rc = git_libgit2_init();
    tried = 1;
    if (rc < 0) {
        const git_error *e = git_error_last();
        ywarn("git_repo: git_libgit2_init failed: %s",
              e && e->message ? e->message : "(no msg)");
        return 0;
    }
    ok = 1;
    ydebug("git_repo: libgit2 initialised (ref-count=%d)", rc);
    return 1;
}

/* Strict path-segment whitelist: lower/upper alpha, digits, dot, dash,
 * underscore; length 1..REPO_NAME_MAX-1; no leading dot (blocks `.git`
 * collisions and dotfile shenanigans). Same shape as the frontend's
 * `username_ok` / `reponame_ok` checks — we re-validate at the sink so
 * a future caller bypassing the frontend can't reach the filesystem
 * with a hostile name. */
static int path_segment_ok(const char *s)
{
    if (!s || !*s) return 0;
    if (s[0] == '.') return 0;
    size_t n = 0;
    for (const char *p = s; *p; ++p, ++n) {
        if (n >= REPO_NAME_MAX - 1) return 0;
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_'))
            return 0;
    }
    return 1;
}

/* Deterministic repo id: FNV-1a of "<owner_name>/<repo_name>" → uint32,
 * 0 nudged to 1 (0 is reserved for "missing"). This MUST match the
 * gateway's hash_repo() in src/picomesh/frontends/yhttp/frontend.c — the
 * gateway computes the id from names on every page (no lookup round-trip)
 * and the services must store/find repos under that same id. Keeping the
 * id derived from names (not a private counter) is what makes
 * read_tree/read_file/put_file reachable from the gateway. */
static uint32_t repo_hash(const char *owner_name, const char *repo_name)
{
    char key[160];
    snprintf(key, sizeof(key), "%s/%s", owner_name, repo_name);
    uint32_t h = 2166136261u;
    for (const char *p = key; *p; ++p) {
        h ^= (unsigned char)*p;
        h *= 16777619u;
    }
    return h ? h : 1;
}

/* Build `<repos_dir>/<owner_name>/<repo_name>.git` into `out`. */
static int repo_dir_build(const char *owner_name, const char *repo_name,
                          char *out, size_t cap)
{
    const char *root = resolve_repos_dir();
    int n = snprintf(out, cap, "%s/%s/%s.git", root, owner_name, repo_name);
    return (n > 0 && (size_t)n < cap) ? 0 : -1;
}

/* mkdir -p for the parent of the repo dir. libgit2's repository_init
 * makes the leaf, but the parent (`repos_dir`) has to exist. */
static int mkdir_p(const char *path)
{
    char buf[1024];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(buf)) return -1;
    memcpy(buf, path, n + 1);
    for (size_t i = 1; i < n; ++i) {
        if (buf[i] == '/') {
            buf[i] = 0;
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            buf[i] = '/';
        }
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* nftw callback: unlink files, rmdir directories. */
static int rm_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st; (void)flag; (void)ftw;
    return remove(path);
}

static int rm_rf(const char *path)
{
    /* FTW_DEPTH: visit children before parents; FTW_PHYS: don't follow
     * symlinks (the bare repo never contains any, but defensive). */
    return nftw(path, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
}

/* The on-disk half of make: create the per-user parent dir and run
 * libgit2's bare init. Both are blocking filesystem calls — on the
 * in-browser RISC-V emulator a single git_repository_init runs for tens
 * of seconds — so this runs on the libuv worker pool (yloop_run_blocking),
 * NOT the loop thread. It therefore touches ONLY its own `arg`: no
 * object, no yconfig, no loop state. Results (rc + a snapshot of
 * libgit2's thread-local error message) come back in the struct; the
 * caller inspects them after the await resumes. */
struct git_init_work {
    char parent[1024];   /* <repos_dir>/<owner_name> — mkdir_p target */
    char path[1024];     /* <repos_dir>/<owner_name>/<repo_name>.git  */
    int  mkdir_errno;    /* 0 on success, else errno from mkdir_p     */
    int  git_rc;         /* libgit2 return code (< 0 is failure)      */
    char git_errmsg[256];/* libgit2 error snapshot (same pool thread) */
};

static void git_init_work_fn(void *arg)
{
    struct git_init_work *w = arg;
    if (mkdir_p(w->parent) != 0) {
        w->mkdir_errno = errno ? errno : -1;
        return;
    }
    git_repository *repo = NULL;
    w->git_rc = git_repository_init(&repo, w->path, /*is_bare*/ 1);
    if (w->git_rc < 0) {
        const git_error *e = git_error_last();
        snprintf(w->git_errmsg, sizeof(w->git_errmsg), "%s",
                 e && e->message ? e->message : "(no msg)");
    } else {
        git_repository_free(repo);
    }
}

/* ---- tree/blob/commit I/O (libgit2, on the worker pool) ----------- *
 *
 * read_tree / read_file / put_file all touch the on-disk bare repo —
 * blocking I/O that must NOT run on the loop thread (a single libgit2
 * call costs tens of seconds under the in-browser emulator). Each has a
 * `*_work_fn` that runs on the libuv worker pool via the same
 * run_blocking_or_inline() dispatch make uses; the work fn touches ONLY
 * its own payload struct (no object, no yconfig, no loop state). Strings
 * handed back in `out` are malloc'd; ownership transfers to the Result
 * the impl returns (picomesh_string contract).                          */

/* Find the metadata row for repo_id (NULL if absent). */
static struct repo_entry *find_entry(struct git_repo_store_data *d, uint32_t repo_id)
{
    for (size_t i = 0; i < REPOS_MAX; ++i)
        if (d->entries[i].used && d->entries[i].repo_id == repo_id)
            return &d->entries[i];
    return NULL;
}

/* Offload `fn(work)` to the worker pool and park this coroutine until it
 * finishes; fall back to inline (degraded, blocking) when there's no
 * serve loop or dispatch fails. Mirrors store_make's pattern. */
static void run_blocking_or_inline(void (*fn)(void *), void *work)
{
    struct picomesh_engine *e = picomesh_active_engine();
    struct yloop *loop = e ? picomesh_engine_loop(e) : NULL;
    if (loop) {
        struct picomesh_void_result br = yloop_run_blocking(loop, fn, work);
        if (PICOMESH_IS_ERR(br)) { picomesh_error_destroy(br.error); fn(work); }
    } else {
        fn(work);
    }
}

struct git_read_work {
    char repo_path[1024];
    char ref[128];
    char path[1024];
    char *out;        /* malloc'd NUL-terminated; ownership → caller */
    size_t out_len;
    int rc;           /* 0 = ok */
};

/* Resolve <ref> (default "HEAD") to its tree. Non-zero return means the
 * ref doesn't resolve — for an empty repo (unborn HEAD) that's expected,
 * and read_tree treats it as "no entries" rather than an error. */
static int resolve_tree(git_repository *repo, const char *ref, git_tree **out_tree)
{
    const char *spec = (ref && *ref) ? ref : "HEAD";
    git_object *obj = NULL;
    if (git_revparse_single(&obj, repo, spec) != 0) return -1;
    git_object *peeled = NULL;
    int rc = git_object_peel(&peeled, obj, GIT_OBJECT_TREE);
    git_object_free(obj);
    if (rc != 0) return rc;
    *out_tree = (git_tree *)peeled;
    return 0;
}

static void git_read_tree_work_fn(void *ud)
{
    struct git_read_work *w = ud;
    w->out = NULL; w->out_len = 0; w->rc = 0;

    git_repository *repo = NULL;
    if (git_repository_open(&repo, w->repo_path) != 0) { w->rc = -1; return; }

    git_tree *tree = NULL;
    if (resolve_tree(repo, w->ref, &tree) != 0) {
        /* Empty/unborn repo → empty listing, not an error. */
        git_repository_free(repo);
        w->out = strdup("");
        w->rc = w->out ? 0 : -1;
        return;
    }

    /* Descend into a subdirectory when path is non-empty. */
    git_tree *subtree = NULL;
    const git_tree *cur = tree;
    if (w->path[0]) {
        git_tree_entry *te = NULL;
        if (git_tree_entry_bypath(&te, tree, w->path) != 0 ||
            git_tree_entry_type(te) != GIT_OBJECT_TREE) {
            if (te) git_tree_entry_free(te);
            git_tree_free(tree); git_repository_free(repo);
            w->rc = -2; /* path is not a directory */
            return;
        }
        int lr = git_tree_lookup(&subtree, repo, git_tree_entry_id(te));
        git_tree_entry_free(te);
        if (lr != 0) { git_tree_free(tree); git_repository_free(repo); w->rc = -3; return; }
        cur = subtree;
    }

    /* One entry per line: "<type>\t<name>\n", type ∈ {tree,blob}. git
     * returns entries name-sorted already. */
    size_t n = git_tree_entrycount(cur);
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    if (buf) {
        buf[0] = 0;
        for (size_t i = 0; i < n; i++) {
            const git_tree_entry *e = git_tree_entry_byindex(cur, i);
            const char *nm = git_tree_entry_name(e);
            const char *ty = git_tree_entry_type(e) == GIT_OBJECT_TREE ? "tree" : "blob";
            size_t need = strlen(ty) + 1 + strlen(nm) + 1;
            if (len + need + 1 > cap) {
                while (len + need + 1 > cap) cap *= 2;
                char *nb = realloc(buf, cap);
                if (!nb) { free(buf); buf = NULL; break; }
                buf = nb;
            }
            len += (size_t)snprintf(buf + len, cap - len, "%s\t%s\n", ty, nm);
        }
    }
    if (buf) { buf[len] = 0; w->out = buf; w->out_len = len; w->rc = 0; }
    else w->rc = -1;

    if (subtree) git_tree_free(subtree);
    git_tree_free(tree);
    git_repository_free(repo);
}

static void git_read_file_work_fn(void *ud)
{
    struct git_read_work *w = ud;
    w->out = NULL; w->out_len = 0; w->rc = 0;

    git_repository *repo = NULL;
    if (git_repository_open(&repo, w->repo_path) != 0) { w->rc = -1; return; }

    git_tree *tree = NULL;
    if (resolve_tree(repo, w->ref, &tree) != 0) { git_repository_free(repo); w->rc = -2; return; }

    git_tree_entry *te = NULL;
    if (git_tree_entry_bypath(&te, tree, w->path) != 0 ||
        git_tree_entry_type(te) != GIT_OBJECT_BLOB) {
        if (te) git_tree_entry_free(te);
        git_tree_free(tree); git_repository_free(repo);
        w->rc = -3; /* not a file */
        return;
    }
    git_blob *blob = NULL;
    int lr = git_blob_lookup(&blob, repo, git_tree_entry_id(te));
    git_tree_entry_free(te);
    if (lr != 0) { git_tree_free(tree); git_repository_free(repo); w->rc = -4; return; }

    size_t sz = (size_t)git_blob_rawsize(blob);
    const void *raw = git_blob_rawcontent(blob);
    char *out = malloc(sz + 1);
    if (out) { memcpy(out, raw, sz); out[sz] = 0; w->out = out; w->out_len = sz; w->rc = 0; }
    else w->rc = -1;

    git_blob_free(blob);
    git_tree_free(tree);
    git_repository_free(repo);
}

struct git_put_work {
    char repo_path[1024];
    char path[1024];
    const char *content;   /* borrowed; alive while the worker runs */
    size_t content_len;
    char message[512];
    char author_name[128];
    char author_email[128];
    char out_oid[GIT_OID_HEXSZ + 1];  /* hex commit id on success */
    int rc;
};

/* Insert blob_oid at comps[idx..n-1] under `base` (NULL = empty tree),
 * writing the resulting tree id to *out. One path component per level,
 * preserving sibling entries of any existing subtree. */
static int put_insert(git_repository *repo, const git_tree *base,
                      char **comps, int idx, int n,
                      const git_oid *blob_oid, git_oid *out)
{
    git_treebuilder *bld = NULL;
    if (git_treebuilder_new(&bld, repo, base) != 0) return -1;

    int rc;
    if (idx == n - 1) {
        rc = git_treebuilder_insert(NULL, bld, comps[idx], blob_oid, GIT_FILEMODE_BLOB);
    } else {
        git_tree *sub = NULL;
        if (base) {
            const git_tree_entry *e = git_treebuilder_get(bld, comps[idx]);
            if (e && git_tree_entry_type(e) == GIT_OBJECT_TREE)
                git_tree_lookup(&sub, repo, git_tree_entry_id(e));
        }
        git_oid sub_oid;
        rc = put_insert(repo, sub, comps, idx + 1, n, blob_oid, &sub_oid);
        if (sub) git_tree_free(sub);
        if (rc == 0)
            rc = git_treebuilder_insert(NULL, bld, comps[idx], &sub_oid, GIT_FILEMODE_TREE);
    }
    if (rc == 0) rc = git_treebuilder_write(out, bld);
    git_treebuilder_free(bld);
    return rc;
}

static void git_put_file_work_fn(void *ud)
{
    struct git_put_work *w = ud;
    w->rc = 0; w->out_oid[0] = 0;

    git_repository *repo = NULL;
    if (git_repository_open(&repo, w->repo_path) != 0) { w->rc = -1; return; }

    /* 1) content → blob */
    git_oid blob_oid;
    if (git_blob_create_from_buffer(&blob_oid, repo, w->content, w->content_len) != 0) {
        git_repository_free(repo); w->rc = -2; return;
    }

    /* 2) the branch HEAD points at + its tip commit + that commit's tree */
    char branch_ref[256];
    snprintf(branch_ref, sizeof(branch_ref), "refs/heads/master");
    git_reference *head = NULL;
    if (git_reference_lookup(&head, repo, "HEAD") == 0) {
        if (git_reference_type(head) == GIT_REFERENCE_SYMBOLIC) {
            const char *t = git_reference_symbolic_target(head);
            if (t) snprintf(branch_ref, sizeof(branch_ref), "%s", t);
        }
        git_reference_free(head);
    }

    git_commit *parent = NULL;
    git_tree *base = NULL;
    git_oid parent_oid;
    int has_parent = (git_reference_name_to_id(&parent_oid, repo, "HEAD") == 0);
    if (has_parent && git_commit_lookup(&parent, repo, &parent_oid) == 0)
        git_commit_tree(&base, parent);

    /* 3) split path into components, build the new root tree */
    char pathbuf[1024];
    snprintf(pathbuf, sizeof(pathbuf), "%s", w->path);
    char *comps[64];
    int n = 0;
    for (char *tok = strtok(pathbuf, "/"); tok && n < 64; tok = strtok(NULL, "/"))
        comps[n++] = tok;
    if (n == 0) {
        if (base) git_tree_free(base);
        if (parent) git_commit_free(parent);
        git_repository_free(repo);
        w->rc = -3; return;
    }

    git_oid tree_oid;
    int rc = put_insert(repo, base, comps, 0, n, &blob_oid, &tree_oid);
    if (base) git_tree_free(base);
    if (rc != 0) {
        if (parent) git_commit_free(parent);
        git_repository_free(repo);
        w->rc = -4; return;
    }

    git_tree *new_tree = NULL;
    if (git_tree_lookup(&new_tree, repo, &tree_oid) != 0) {
        if (parent) git_commit_free(parent);
        git_repository_free(repo);
        w->rc = -5; return;
    }

    /* 4) signature + commit (updates branch_ref, so HEAD advances) */
    git_signature *sig = NULL;
    const char *an = w->author_name[0] ? w->author_name : "picoforge";
    const char *ae = w->author_email[0] ? w->author_email : "picoforge@localhost";
    if (git_signature_now(&sig, an, ae) != 0) {
        git_tree_free(new_tree);
        if (parent) git_commit_free(parent);
        git_repository_free(repo);
        w->rc = -6; return;
    }

    git_oid commit_oid;
    const git_commit *parents[1] = { parent };
    rc = git_commit_create(&commit_oid, repo, branch_ref, sig, sig, NULL,
                           w->message[0] ? w->message : "update", new_tree,
                           has_parent ? 1 : 0, has_parent ? parents : NULL);
    if (rc == 0) git_oid_tostr(w->out_oid, sizeof(w->out_oid), &commit_oid);
    else w->rc = -7;

    git_signature_free(sig);
    git_tree_free(new_tree);
    if (parent) git_commit_free(parent);
    git_repository_free(repo);
}

PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_make")
struct picomesh_uint32_result git_repo_store_make_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                      uint32_t owner_id,
                                                      const char *owner_name,
                                                      const char *repo_name)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    if (!path_segment_ok(owner_name) || !path_segment_ok(repo_name)) {
        return PICOMESH_ERR(picomesh_uint32, "git_repo_make: invalid owner_name/repo_name");
    }

    /* Refuse to make a second repo with the same (owner_name, repo_name).
     * The on-disk dir would collide and git_repository_init would either
     * clobber or fail half-way; better to reject up front. */
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used &&
            strcmp(d->entries[i].owner_name, owner_name) == 0 &&
            strcmp(d->entries[i].repo_name,  repo_name)  == 0) {
            return PICOMESH_ERR(picomesh_uint32, "git_repo_make: repo already exists");
        }
    }

    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (!d->entries[i].used) {
            /* Id is derived from the names (see repo_hash) so the gateway
             * and every service agree on it without a lookup. The dup
             * check above already rejected a same-(owner,name) repo, so a
             * clash here would be a genuine FNV-1a collision across two
             * different names — astronomically unlikely; we don't special-
             * case it. (next_id is retained in the struct but unused.) */
            uint32_t repo_id = repo_hash(owner_name, repo_name);

            /* Reserve the metadata slot BEFORE the disk init: if init
             * fails we roll back the slot. Doing it the other way lets
             * a leaked on-disk dir survive across crashes with no
             * matching metadata row. */
            d->entries[i].repo_id = repo_id;
            d->entries[i].owner_id = owner_id;
            snprintf(d->entries[i].owner_name, REPO_NAME_MAX, "%s", owner_name);
            snprintf(d->entries[i].repo_name,  REPO_NAME_MAX, "%s", repo_name);
            d->entries[i].is_public = 0;  /* private by default; set_public opts in */
            d->entries[i].used = 1;
            d->count++;

            /* On-disk bare repo (libgit2) — needed for real git
             * push/clone, but NOT for the HTML UI, and crippling slow on
             * the in-browser emulated disk. Gated by `git_repo.disk_init`
             * so it can be turned off entirely; real deployments leave it
             * on. The blocking work (mkdir_p + git_repository_init) runs
             * on the libuv worker pool via yloop_run_blocking, so the
             * event loop keeps serving other connections instead of
             * freezing for the tens of seconds a git init costs under the
             * in-browser emulator. */
            if (repo_disk_init_enabled()) {
                /* libgit2 runtime init stays on the loop thread (the work
                 * fn must touch only its own arg). */
                if (!ensure_libgit2()) {
                    d->entries[i].used = 0; d->count--;
                    return PICOMESH_ERR(picomesh_uint32, "git_repo_make: libgit2 init failed");
                }
                struct git_init_work work = {0};
                if (repo_dir_build(owner_name, repo_name, work.path, sizeof(work.path)) != 0) {
                    d->entries[i].used = 0; d->count--;
                    return PICOMESH_ERR(picomesh_uint32, "git_repo_make: path too long");
                }
                /* `<repos_dir>/<owner_name>/` — the per-user parent the
                 * pool thread mkdir_p's before libgit2 fills in the leaf. */
                int pn = snprintf(work.parent, sizeof(work.parent), "%s/%s",
                                  resolve_repos_dir(), owner_name);
                if (pn <= 0 || (size_t)pn >= sizeof(work.parent)) {
                    d->entries[i].used = 0; d->count--;
                    return PICOMESH_ERR(picomesh_uint32, "git_repo_make: path too long");
                }

                /* Offload to the worker pool and park this coroutine until
                 * it finishes. `work` lives on this (suspended) coro's
                 * stack — stable while the pool thread writes to it. */
                struct picomesh_engine *e = picomesh_active_engine();
                struct yloop *loop = e ? picomesh_engine_loop(e) : NULL;
                if (loop) {
                    struct picomesh_void_result br =
                        yloop_run_blocking(loop, git_init_work_fn, &work);
                    if (PICOMESH_IS_ERR(br)) {
                        /* Couldn't even dispatch the work — run inline so
                         * the repo still gets created (degraded, blocking). */
                        picomesh_error_destroy(br.error);
                        git_init_work_fn(&work);
                    }
                } else {
                    /* No serve loop (bootstrap/tests): run inline. */
                    git_init_work_fn(&work);
                }

                if (work.mkdir_errno != 0) {
                    d->entries[i].used = 0; d->count--;
                    ywarn("git_repo: cannot create per-user parent %s: %s",
                          work.parent, strerror(work.mkdir_errno));
                    return PICOMESH_ERR(picomesh_uint32, "git_repo_make: mkdir parent failed");
                }
                if (work.git_rc < 0) {
                    ywarn("git_repo: init(%s) failed: %s", work.path, work.git_errmsg);
                    d->entries[i].used = 0; d->count--;
                    return PICOMESH_ERR(picomesh_uint32, "git_repo_make: libgit2 init failed");
                }
                yinfo("git_repo: created repo=%u %s/%s at %s",
                      repo_id, owner_name, repo_name, work.path);
            } else {
                yinfo("git_repo: recorded repo=%u %s/%s (disk_init off)",
                      repo_id, owner_name, repo_name);
            }
            return PICOMESH_OK(picomesh_uint32, repo_id);
        }
    }
    return PICOMESH_ERR(picomesh_uint32, "git_repo_create: table full");
}

PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_delete")
struct picomesh_int_result git_repo_store_delete_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                   uint32_t repo_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].repo_id == repo_id) {
            /* Names recorded at make time tell us exactly which on-disk
             * dir belongs to this repo_id — no guessing from the id. */
            char path[1024];
            if (repo_dir_build(d->entries[i].owner_name,
                               d->entries[i].repo_name,
                               path, sizeof(path)) == 0) {
                if (rm_rf(path) != 0 && errno != ENOENT) {
                    ywarn("git_repo: rm_rf(%s) failed: %s",
                          path, strerror(errno));
                }
            }
            d->entries[i].used = 0;
            d->count--;
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_OK(picomesh_int, 0);
}

PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_owner_of")
struct picomesh_uint32_result git_repo_store_owner_of_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                        uint32_t repo_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].repo_id == repo_id) {
            return PICOMESH_OK(picomesh_uint32, d->entries[i].owner_id);
        }
    }
    return PICOMESH_OK(picomesh_uint32, 0);
}

PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_count_for_owner")
struct picomesh_size_result git_repo_store_count_for_owner_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                             uint32_t owner_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    size_t n = 0;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].owner_id == owner_id) n++;
    }
    return PICOMESH_OK(picomesh_size, n);
}

PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_count_total")
struct picomesh_size_result git_repo_store_count_total_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return PICOMESH_OK(picomesh_size, gr(obj)->count);
}

/* List the repo names owned by `owner_id`, newline-separated (empty
 * string if none). The caller (gateway namespace/repos pages) splits on
 * '\n' and renders each as a link — without this there was no way to
 * enumerate a user's repos by NAME, so created repos never showed up in
 * any listing. Heap string; the caller owns and frees it (picomesh_string
 * contract). */
PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_list_for_owner")
struct picomesh_string_result git_repo_store_list_for_owner_impl(struct ctx *ctx, struct object *obj,
                                                              struct yheaders *hdrs, uint32_t owner_id)
{
    (void)ctx; (void)hdrs;
    struct git_repo_store_data *d = gr(obj);
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return PICOMESH_ERR(picomesh_string, "git_repo_list_for_owner: out of memory");
    out[0] = 0;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (!d->entries[i].used || d->entries[i].owner_id != owner_id) continue;
        size_t nl = strlen(d->entries[i].repo_name);
        if (len + nl + 2 > cap) {
            while (len + nl + 2 > cap) cap *= 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(out); return PICOMESH_ERR(picomesh_string, "git_repo_list_for_owner: out of memory"); }
            out = nb;
        }
        memcpy(out + len, d->entries[i].repo_name, nl);
        len += nl;
        out[len++] = '\n';
        out[len] = 0;
    }
    return PICOMESH_OK(picomesh_string, out);
}

/* ---- tree/blob/commit public methods ------------------------------ */

/* List a directory in the repo tree. `ref` defaults to HEAD; `path` ""
 * is the root. Returns "<type>\t<name>\n" lines (type ∈ tree|blob); an
 * empty repo yields an empty string. */
PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_read_tree")
struct picomesh_string_result git_repo_store_read_tree_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                            uint32_t repo_id, const char *ref, const char *path)
{
    (void)ctx;
    struct repo_entry *e = find_entry(gr(obj), repo_id);
    if (!e) return PICOMESH_ERR(picomesh_string, "git_repo_read_tree: no such repo");
    /* Read authz: public repos are world-readable; private ones only by
     * the owner. uid comes from the trusted hdrs prefix the gateway set
     * from the session (NULL hdrs ⇒ in-process caller ⇒ uid 0). */
    uint32_t uid = hdrs ? yheaders_get_u32(hdrs, "uid", 0) : 0;
    if (!e->is_public && uid != e->owner_id)
        return PICOMESH_ERR(picomesh_string, "git_repo_read_tree: forbidden (private repo)");
    if (!ensure_libgit2()) return PICOMESH_ERR(picomesh_string, "git_repo_read_tree: libgit2 init failed");

    struct git_read_work w;
    memset(&w, 0, sizeof(w));
    if (repo_dir_build(e->owner_name, e->repo_name, w.repo_path, sizeof(w.repo_path)) != 0)
        return PICOMESH_ERR(picomesh_string, "git_repo_read_tree: path too long");
    snprintf(w.ref, sizeof(w.ref), "%s", ref ? ref : "");
    snprintf(w.path, sizeof(w.path), "%s", path ? path : "");

    run_blocking_or_inline(git_read_tree_work_fn, &w);
    if (w.rc != 0 || !w.out) { free(w.out); return PICOMESH_ERR(picomesh_string, "git_repo_read_tree: not a directory or git error"); }
    return PICOMESH_OK(picomesh_string, w.out);
}

/* Read a file's contents from the repo tree. `ref` defaults to HEAD.
 * Text-oriented: the result is NUL-terminated, so an embedded NUL in a
 * binary blob would truncate downstream consumers. */
PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_read_file")
struct picomesh_string_result git_repo_store_read_file_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                            uint32_t repo_id, const char *ref, const char *path)
{
    (void)ctx;
    if (!path || !*path) return PICOMESH_ERR(picomesh_string, "git_repo_read_file: path required");
    struct repo_entry *e = find_entry(gr(obj), repo_id);
    if (!e) return PICOMESH_ERR(picomesh_string, "git_repo_read_file: no such repo");
    /* Same read authz as read_tree: public → anyone, private → owner only. */
    uint32_t uid = hdrs ? yheaders_get_u32(hdrs, "uid", 0) : 0;
    if (!e->is_public && uid != e->owner_id)
        return PICOMESH_ERR(picomesh_string, "git_repo_read_file: forbidden (private repo)");
    if (!ensure_libgit2()) return PICOMESH_ERR(picomesh_string, "git_repo_read_file: libgit2 init failed");

    struct git_read_work w;
    memset(&w, 0, sizeof(w));
    if (repo_dir_build(e->owner_name, e->repo_name, w.repo_path, sizeof(w.repo_path)) != 0)
        return PICOMESH_ERR(picomesh_string, "git_repo_read_file: path too long");
    snprintf(w.ref, sizeof(w.ref), "%s", ref ? ref : "");
    snprintf(w.path, sizeof(w.path), "%s", path);

    run_blocking_or_inline(git_read_file_work_fn, &w);
    if (w.rc != 0 || !w.out) { free(w.out); return PICOMESH_ERR(picomesh_string, "git_repo_read_file: not a file or git error"); }
    return PICOMESH_OK(picomesh_string, w.out);
}

/* Create or overwrite a file at `path` (nested dirs are created in the
 * tree as needed — git has no standalone mkdir) and commit it on the
 * branch HEAD points at. Works on an empty repo (first commit). Returns
 * the new commit's hex id. */
PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_put_file")
struct picomesh_string_result git_repo_store_put_file_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                           uint32_t repo_id, const char *path, const char *content,
                                                           const char *message, const char *author_name,
                                                           const char *author_email)
{
    (void)ctx;
    if (!path || !*path) return PICOMESH_ERR(picomesh_string, "git_repo_put_file: path required");
    struct repo_entry *e = find_entry(gr(obj), repo_id);
    if (!e) return PICOMESH_ERR(picomesh_string, "git_repo_put_file: no such repo");
    /* Write authz: owner-only, always (independent of public/private).
     * Anonymous (uid 0) is always refused. */
    uint32_t uid = hdrs ? yheaders_get_u32(hdrs, "uid", 0) : 0;
    if (uid == 0 || uid != e->owner_id)
        return PICOMESH_ERR(picomesh_string, "git_repo_put_file: forbidden (not repo owner)");
    if (!ensure_libgit2()) return PICOMESH_ERR(picomesh_string, "git_repo_put_file: libgit2 init failed");

    struct git_put_work w;
    memset(&w, 0, sizeof(w));
    if (repo_dir_build(e->owner_name, e->repo_name, w.repo_path, sizeof(w.repo_path)) != 0)
        return PICOMESH_ERR(picomesh_string, "git_repo_put_file: path too long");
    snprintf(w.path, sizeof(w.path), "%s", path);
    w.content = content ? content : "";
    w.content_len = content ? strlen(content) : 0;
    snprintf(w.message, sizeof(w.message), "%s", message ? message : "");
    snprintf(w.author_name, sizeof(w.author_name), "%s", author_name ? author_name : "");
    snprintf(w.author_email, sizeof(w.author_email), "%s", author_email ? author_email : "");

    run_blocking_or_inline(git_put_file_work_fn, &w);
    if (w.rc != 0 || !w.out_oid[0]) return PICOMESH_ERR(picomesh_string, "git_repo_put_file: git error");
    char *oid = strdup(w.out_oid);
    if (!oid) return PICOMESH_ERR(picomesh_string, "git_repo_put_file: out of memory");
    return PICOMESH_OK(picomesh_string, oid);
}

/* ---- visibility (public/private flag) ----------------------------- */

/* Read the repo's visibility: 1 = public, 0 = private. No authz — the
 * flag itself isn't a secret (whether a repo exists/its name already
 * leaks via list_for_owner), and the gateway needs it to decide how to
 * render. The CONTENTS stay gated by read_tree/read_file. Returns 0 for
 * an unknown repo (treated as not-public). */
PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_is_public")
struct picomesh_int_result git_repo_store_is_public_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t repo_id)
{
    (void)ctx; (void)hdrs;
    struct repo_entry *e = find_entry(gr(obj), repo_id);
    return PICOMESH_OK(picomesh_int, e ? (e->is_public ? 1 : 0) : 0);
}

/* Set the repo's visibility (1 = public, 0 = private). Owner-only;
 * anonymous (uid 0) is refused. Returns 1 on success. */
PICOMESH_CLASS_ANNOTATE("override@git_repo:store:store_set_public")
struct picomesh_int_result git_repo_store_set_public_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                          uint32_t repo_id, int is_public)
{
    (void)ctx;
    struct repo_entry *e = find_entry(gr(obj), repo_id);
    if (!e) return PICOMESH_ERR(picomesh_int, "git_repo_set_public: no such repo");
    uint32_t uid = hdrs ? yheaders_get_u32(hdrs, "uid", 0) : 0;
    if (uid == 0 || uid != e->owner_id)
        return PICOMESH_ERR(picomesh_int, "git_repo_set_public: forbidden (not repo owner)");
    e->is_public = is_public ? 1 : 0;
    yinfo("git_repo: repo=%u visibility -> %s", repo_id, e->is_public ? "public" : "private");
    return PICOMESH_OK(picomesh_int, 1);
}

#include "store.gen.c"
