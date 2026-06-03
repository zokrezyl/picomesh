/* issues — bug tracker.
 *
 *   open(repo_id, author_id)              → issue_id
 *   close(issue_id)                       → 1 closed / 0 unknown
 *   status(issue_id)                      → 0 unknown / 1 open / 2 closed
 *   count_open_in_repo(repo_id)           → number of open issues
 *   count_total                           → total tracked
 *
 * State lives in the shared `sharded_storage` service (context `issues`),
 * NOT in this process — so multiple issues objects / gateway workers share
 * one source of truth with nothing cached in memory to fragment.
 *
 * Storage layout in the `issues` context:
 *   next_id           → monotonic issue-id counter
 *   count             → total issues ever opened
 *   issue:<id>        → "<repo_id>\t<author_id>\t<closed>"
 *   open:<repo_id>    → live open-issue count for that repo
 */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/plugin/sharded_storage/sharded_storage.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISSUES_CTX "issues"

/* No in-memory state — every op delegates to storage. */
struct PICOMESH_CLASS_ANNOTATE("class@issues:issues") issues_issues_data {
    char _unused;
};

struct is_storage {
    struct ctx c;
    struct object *obj;
};
PICOMESH_RESULT_DECLARE(is_storage, struct is_storage);

static struct is_storage_result is_open_storage(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(is_storage, "issues: no active engine");
    struct is_storage h = {.c = picomesh_engine_service_ctx(e, "sharded_storage")};
    struct object_ptr_result o = sharded_storage_db_create(&h.c);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(is_storage, "issues: storage_db_create failed", o);
    h.obj = o.value;
    return PICOMESH_OK(is_storage, h);
}

/* Read an int. A real backend error is propagated; an absent/empty key
 * (db_get → "") yields `fallback` — the two are NOT conflated. */
static struct picomesh_int64_result is_get(struct is_storage *h, struct yheaders *hdrs, const char *key, int64_t fallback)
{
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, ISSUES_CTX, key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "issues: storage read failed", r);
    int64_t v = (r.value && r.value[0]) ? strtoll(r.value, NULL, 10) : fallback;
    free(r.value);
    return PICOMESH_OK(picomesh_int64, v);
}

/* Atomic counter / id bump — OK value is the value after the add. Storage
 * serializes the read-add-write so concurrent opens never collide on an id;
 * a backend failure is propagated, never collapsed into a (valid) 0/id. */
static struct picomesh_int64_result is_incr(struct is_storage *h, struct yheaders *hdrs, const char *key, int64_t delta)
{
    struct picomesh_int64_result r = sharded_storage_db_incr(&h->c, h->obj, hdrs, ISSUES_CTX, key, delta);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "issues: counter update failed", r);
    return r;
}

/* Atomic compare-and-set on a row. OK value: 1 = swapped, 0 = compare
 * mismatch (a concurrent writer got there first). A backend error is
 * propagated — distinct from a clean mismatch. */
static struct picomesh_int_result is_cas(struct is_storage *h, struct yheaders *hdrs, const char *key,
                                         const char *expected, const char *replacement)
{
    struct picomesh_int_result r =
        sharded_storage_db_compare_and_set(&h->c, h->obj, hdrs, ISSUES_CTX, key, expected, replacement);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int, "issues: compare-and-set failed", r);
    return r;
}

/* Load issue:<id> → repo_id/author_id/closed. OK value: 1 = present (out
 * params filled), 0 = absent. A backend read failure is propagated —
 * distinct from "no such issue". */
static struct picomesh_int_result issue_load(struct is_storage *h, struct yheaders *hdrs, uint32_t issue_id,
                                             uint32_t *repo_id, uint32_t *author_id, int *closed)
{
    char k[40];
    snprintf(k, sizeof(k), "issue:%u", issue_id);
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, ISSUES_CTX, k);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int, "issues: issue read failed", r);
    if (!r.value || !r.value[0]) { free(r.value); return PICOMESH_OK(picomesh_int, 0); }
    char *s = r.value, *t1 = strchr(s, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2) { free(s); return PICOMESH_OK(picomesh_int, 0); }
    *t1 = *t2 = 0;
    if (repo_id) *repo_id = (uint32_t)strtoul(s, NULL, 10);
    if (author_id) *author_id = (uint32_t)strtoul(t1 + 1, NULL, 10);
    if (closed) *closed = atoi(t2 + 1);
    free(s);
    return PICOMESH_OK(picomesh_int, 1);
}

/* Write the canonical issue row; propagates a failed write. */
static struct picomesh_void_result issue_store(struct is_storage *h, struct yheaders *hdrs, uint32_t issue_id,
                                               uint32_t repo_id, uint32_t author_id, int closed)
{
    char k[40], v[48];
    snprintf(k, sizeof(k), "issue:%u", issue_id);
    snprintf(v, sizeof(v), "%u\t%u\t%d", repo_id, author_id, closed ? 1 : 0);
    struct picomesh_int_result r = sharded_storage_db_set(&h->c, h->obj, hdrs, ISSUES_CTX, k, v);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "issues: issue write failed", r);
    return PICOMESH_OK_VOID();
}

PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_open")
struct picomesh_uint32_result issues_issues_open_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                  uint32_t repo_id, uint32_t author_id)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "issues_open: storage open failed", sr);
    struct is_storage h = sr.value;

    struct picomesh_int64_result idr = is_incr(&h, hdrs, "next_id", 1);
    if (PICOMESH_IS_ERR(idr)) return PICOMESH_ERR(picomesh_uint32, "issues_open: allocate id failed", idr);
    uint32_t id = (uint32_t)idr.value;
    struct picomesh_void_result ws = issue_store(&h, hdrs, id, repo_id, author_id, 0);
    if (PICOMESH_IS_ERR(ws)) return PICOMESH_ERR(picomesh_uint32, "issues_open: persist issue failed", ws);
    struct picomesh_int64_result cc = is_incr(&h, hdrs, "count", 1);
    if (PICOMESH_IS_ERR(cc)) return PICOMESH_ERR(picomesh_uint32, "issues_open: bump count failed", cc);
    char ok[40];
    snprintf(ok, sizeof(ok), "open:%u", repo_id);
    struct picomesh_int64_result oc = is_incr(&h, hdrs, ok, 1);
    if (PICOMESH_IS_ERR(oc)) return PICOMESH_ERR(picomesh_uint32, "issues_open: bump open count failed", oc);
    yinfo("issues: open id=%u repo=%u by=%u", id, repo_id, author_id);
    return PICOMESH_OK(picomesh_uint32, id);
}

PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_close")
struct picomesh_int_result issues_issues_close_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                uint32_t issue_id)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "issues_close: storage open failed", sr);
    struct is_storage h = sr.value;

    uint32_t repo_id = 0, author_id = 0; int closed = 0;
    struct picomesh_int_result lr = issue_load(&h, hdrs, issue_id, &repo_id, &author_id, &closed);
    if (PICOMESH_IS_ERR(lr)) return PICOMESH_ERR(picomesh_int, "issues_close: load failed", lr);
    if (lr.value == 0 || closed) return PICOMESH_OK(picomesh_int, 0);  /* absent or already closed */
    /* Flip closed 0→1 atomically. issue_store writes a canonical
     * "<repo>\t<author>\t<closed>" row, so the expected bytes are exactly
     * reconstructable. A clean CAS mismatch (value 0) means another caller
     * already closed it → 0, no double-decrement. A backend error propagates. */
    char k[40], expected[48], replacement[48];
    snprintf(k, sizeof(k), "issue:%u", issue_id);
    snprintf(expected, sizeof(expected), "%u\t%u\t0", repo_id, author_id);
    snprintf(replacement, sizeof(replacement), "%u\t%u\t1", repo_id, author_id);
    struct picomesh_int_result cas = is_cas(&h, hdrs, k, expected, replacement);
    if (PICOMESH_IS_ERR(cas)) return PICOMESH_ERR(picomesh_int, "issues_close: close CAS failed", cas);
    if (cas.value == 0) return PICOMESH_OK(picomesh_int, 0);  /* lost the race — already closed */
    char ok[40];
    snprintf(ok, sizeof(ok), "open:%u", repo_id);
    struct picomesh_int64_result oc = is_incr(&h, hdrs, ok, -1);
    if (PICOMESH_IS_ERR(oc)) return PICOMESH_ERR(picomesh_int, "issues_close: open count update failed", oc);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_status")
struct picomesh_int_result issues_issues_status_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                 uint32_t issue_id)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "issues_status: storage open failed", sr);
    struct is_storage h = sr.value;
    int closed = 0;
    struct picomesh_int_result lr = issue_load(&h, hdrs, issue_id, NULL, NULL, &closed);
    if (PICOMESH_IS_ERR(lr)) return PICOMESH_ERR(picomesh_int, "issues_status: load failed", lr);
    if (lr.value == 0) return PICOMESH_OK(picomesh_int, 0);  /* unknown issue */
    return PICOMESH_OK(picomesh_int, closed ? 2 : 1);
}

PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_count_open_in_repo")
struct picomesh_size_result issues_issues_count_open_in_repo_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                              uint32_t repo_id)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "issues_count_open: storage open failed", sr);
    struct is_storage h = sr.value;
    char ok[40];
    snprintf(ok, sizeof(ok), "open:%u", repo_id);
    struct picomesh_int64_result nr = is_get(&h, hdrs, ok, 0);
    if (PICOMESH_IS_ERR(nr)) return PICOMESH_ERR(picomesh_size, "issues_count_open: read failed", nr);
    return PICOMESH_OK(picomesh_size, (size_t)(nr.value < 0 ? 0 : nr.value));
}

PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_count_total")
struct picomesh_size_result issues_issues_count_total_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "issues_count_total: storage open failed", sr);
    struct is_storage h = sr.value;
    struct picomesh_int64_result nr = is_get(&h, hdrs, "count", 0);
    if (PICOMESH_IS_ERR(nr)) return PICOMESH_ERR(picomesh_size, "issues_count_total: read failed", nr);
    return PICOMESH_OK(picomesh_size, (size_t)(nr.value < 0 ? 0 : nr.value));
}

/* List ALL issues' stored entries as a JSON array (gh#15) — every object,
 * not scoped per repo, not a count. Delegates to the namespace scan. */
PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_list")
struct picomesh_json_result issues_issues_list_impl(struct ctx *ctx, struct object *obj,
                                                   struct yheaders *hdrs,
                                                   int64_t offset, int64_t limit)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "issues_list: storage open failed", sr);
    struct is_storage h = sr.value;
    return sharded_storage_db_list(&h.c, h.obj, hdrs, ISSUES_CTX, "issue:", offset, limit);
}

/* Unbounded variant — every issue. Use with care on large deployments. */
PICOMESH_CLASS_ANNOTATE("override@issues:issues:issues_list_all")
struct picomesh_json_result issues_issues_list_all_impl(struct ctx *ctx, struct object *obj,
                                                        struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct is_storage_result sr = is_open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "issues_list_all: storage open failed", sr);
    struct is_storage h = sr.value;
    return sharded_storage_db_list_all(&h.c, h.obj, hdrs, ISSUES_CTX, "issue:");
}

#include "store.gen.c"
