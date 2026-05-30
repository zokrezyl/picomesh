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
 * `/tmp/git-yaafc/repos`). `owner_name` and `repo_name` are validated
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

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/class.h>
#include <yaafc/yengine/engine.h>
#include <yaafc/yconfig/yconfig.h>
#include <yaafc/yloop/yloop.h>

#include <git2.h>

#include <errno.h>
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define REPOS_MAX     256
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
    int used;
};

struct YAAFC_CLASS_ANNOTATE("class@git_repo:store") git_repo_store_data {
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
    struct yaafc_engine *e = yaafc_active_engine();
    if (e) {
        struct yconfig_node_ptr_result r =
            yconfig_get(yaafc_engine_config(e), "git_repo.repos_dir");
        if (YAAFC_IS_OK(r) && r.value) {
            const char *s = yconfig_node_as_string(r.value, NULL);
            if (s && *s) return s;
        }
    }
    return "/tmp/git-yaafc/repos";
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
    struct yaafc_engine *e = yaafc_active_engine();
    if (e) {
        struct yconfig_node_ptr_result r =
            yconfig_get(yaafc_engine_config(e), "git_repo.disk_init");
        if (YAAFC_IS_OK(r) && r.value)
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

YAAFC_CLASS_ANNOTATE("override@git_repo:store:store_make")
struct yaafc_uint32_result git_repo_store_make_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                      uint32_t owner_id,
                                                      const char *owner_name,
                                                      const char *repo_name)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    if (!path_segment_ok(owner_name) || !path_segment_ok(repo_name)) {
        return YAAFC_ERR(yaafc_uint32, "git_repo_make: invalid owner_name/repo_name");
    }

    /* Refuse to make a second repo with the same (owner_name, repo_name).
     * The on-disk dir would collide and git_repository_init would either
     * clobber or fail half-way; better to reject up front. */
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used &&
            strcmp(d->entries[i].owner_name, owner_name) == 0 &&
            strcmp(d->entries[i].repo_name,  repo_name)  == 0) {
            return YAAFC_ERR(yaafc_uint32, "git_repo_make: repo already exists");
        }
    }

    if (d->next_id == 0) d->next_id = 1;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (!d->entries[i].used) {
            uint32_t repo_id = d->next_id++;

            /* Reserve the metadata slot BEFORE the disk init: if init
             * fails we roll back the slot. Doing it the other way lets
             * a leaked on-disk dir survive across crashes with no
             * matching metadata row. */
            d->entries[i].repo_id = repo_id;
            d->entries[i].owner_id = owner_id;
            snprintf(d->entries[i].owner_name, REPO_NAME_MAX, "%s", owner_name);
            snprintf(d->entries[i].repo_name,  REPO_NAME_MAX, "%s", repo_name);
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
                    return YAAFC_ERR(yaafc_uint32, "git_repo_make: libgit2 init failed");
                }
                struct git_init_work work = {0};
                if (repo_dir_build(owner_name, repo_name, work.path, sizeof(work.path)) != 0) {
                    d->entries[i].used = 0; d->count--;
                    return YAAFC_ERR(yaafc_uint32, "git_repo_make: path too long");
                }
                /* `<repos_dir>/<owner_name>/` — the per-user parent the
                 * pool thread mkdir_p's before libgit2 fills in the leaf. */
                int pn = snprintf(work.parent, sizeof(work.parent), "%s/%s",
                                  resolve_repos_dir(), owner_name);
                if (pn <= 0 || (size_t)pn >= sizeof(work.parent)) {
                    d->entries[i].used = 0; d->count--;
                    return YAAFC_ERR(yaafc_uint32, "git_repo_make: path too long");
                }

                /* Offload to the worker pool and park this coroutine until
                 * it finishes. `work` lives on this (suspended) coro's
                 * stack — stable while the pool thread writes to it. */
                struct yaafc_engine *e = yaafc_active_engine();
                struct yloop *loop = e ? yaafc_engine_loop(e) : NULL;
                if (loop) {
                    struct yaafc_void_result br =
                        yloop_run_blocking(loop, git_init_work_fn, &work);
                    if (YAAFC_IS_ERR(br)) {
                        /* Couldn't even dispatch the work — run inline so
                         * the repo still gets created (degraded, blocking). */
                        yaafc_error_destroy(br.error);
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
                    return YAAFC_ERR(yaafc_uint32, "git_repo_make: mkdir parent failed");
                }
                if (work.git_rc < 0) {
                    ywarn("git_repo: init(%s) failed: %s", work.path, work.git_errmsg);
                    d->entries[i].used = 0; d->count--;
                    return YAAFC_ERR(yaafc_uint32, "git_repo_make: libgit2 init failed");
                }
                yinfo("git_repo: created repo=%u %s/%s at %s",
                      repo_id, owner_name, repo_name, work.path);
            } else {
                yinfo("git_repo: recorded repo=%u %s/%s (disk_init off)",
                      repo_id, owner_name, repo_name);
            }
            return YAAFC_OK(yaafc_uint32, repo_id);
        }
    }
    return YAAFC_ERR(yaafc_uint32, "git_repo_create: table full");
}

YAAFC_CLASS_ANNOTATE("override@git_repo:store:store_delete")
struct yaafc_int_result git_repo_store_delete_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
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
            return YAAFC_OK(yaafc_int, 1);
        }
    }
    return YAAFC_OK(yaafc_int, 0);
}

YAAFC_CLASS_ANNOTATE("override@git_repo:store:store_owner_of")
struct yaafc_uint32_result git_repo_store_owner_of_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                        uint32_t repo_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].repo_id == repo_id) {
            return YAAFC_OK(yaafc_uint32, d->entries[i].owner_id);
        }
    }
    return YAAFC_OK(yaafc_uint32, 0);
}

YAAFC_CLASS_ANNOTATE("override@git_repo:store:store_count_for_owner")
struct yaafc_size_result git_repo_store_count_for_owner_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                             uint32_t owner_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    size_t n = 0;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].owner_id == owner_id) n++;
    }
    return YAAFC_OK(yaafc_size, n);
}

YAAFC_CLASS_ANNOTATE("override@git_repo:store:store_count_total")
struct yaafc_size_result git_repo_store_count_total_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return YAAFC_OK(yaafc_size, gr(obj)->count);
}

/* List the repo names owned by `owner_id`, newline-separated (empty
 * string if none). The caller (gateway namespace/repos pages) splits on
 * '\n' and renders each as a link — without this there was no way to
 * enumerate a user's repos by NAME, so created repos never showed up in
 * any listing. Heap string; the caller owns and frees it (yaafc_string
 * contract). */
YAAFC_CLASS_ANNOTATE("override@git_repo:store:store_list_for_owner")
struct yaafc_string_result git_repo_store_list_for_owner_impl(struct ctx *ctx, struct object *obj,
                                                              struct yheaders *hdrs, uint32_t owner_id)
{
    (void)ctx; (void)hdrs;
    struct git_repo_store_data *d = gr(obj);
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return YAAFC_ERR(yaafc_string, "git_repo_list_for_owner: out of memory");
    out[0] = 0;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (!d->entries[i].used || d->entries[i].owner_id != owner_id) continue;
        size_t nl = strlen(d->entries[i].repo_name);
        if (len + nl + 2 > cap) {
            while (len + nl + 2 > cap) cap *= 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(out); return YAAFC_ERR(yaafc_string, "git_repo_list_for_owner: out of memory"); }
            out = nb;
        }
        memcpy(out + len, d->entries[i].repo_name, nl);
        len += nl;
        out[len++] = '\n';
        out[len] = 0;
    }
    return YAAFC_OK(yaafc_string, out);
}

#include "store.gen.c"
