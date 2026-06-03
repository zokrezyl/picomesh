/* git_pipeline — CI job queue.
 *
 *   enqueue(repo_id)        → job_id
 *   lease(runner_id)        → job_id (0 if queue empty)
 *   complete(job_id, status)→ 1 ok / 0 unknown
 *   count_pending           → still in queued state
 *   count_running           → leased but not yet completed
 *   count_done              → completed
 *
 * Status: 0=queued, 1=running, 2=succeeded, 3=failed.
 *
 * State lives in the shared `sharded_storage` service (context
 * `git_pipeline`), NOT in this process — so multiple objects / gateway
 * workers share one source of truth with nothing cached in memory.
 *
 * Storage layout in the `git_pipeline` context:
 *   next_id        → monotonic job-id counter
 *   job:<id>       → "<repo_id>\t<runner_id>\t<status>"
 *   pending/running/done → live counts by state
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

#define PIPE_CTX "git_pipeline"

/* No in-memory state — every op delegates to storage. */
struct PICOMESH_CLASS_ANNOTATE("class@git_pipeline:git_pipeline") git_pipeline_git_pipeline_data {
    char _unused;
};

struct gp_storage {
    struct ctx c;
    struct object *obj;
};
PICOMESH_RESULT_DECLARE(gp_storage, struct gp_storage);

static struct gp_storage_result gp_open(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(gp_storage, "git_pipeline: no active engine");
    struct gp_storage h = {.c = picomesh_engine_service_ctx(e, "sharded_storage")};
    struct object_ptr_result o = sharded_storage_db_create(&h.c);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(gp_storage, "git_pipeline: storage_db_create failed", o);
    h.obj = o.value;
    return PICOMESH_OK(gp_storage, h);
}

/* Read an int. A real backend error is propagated; an absent/empty key
 * (db_get → "") yields `fallback`. */
static struct picomesh_int64_result gp_get(struct gp_storage *h, struct yheaders *hdrs, const char *key, int64_t fallback)
{
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, PIPE_CTX, key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "git_pipeline: storage read failed", r);
    int64_t v = (r.value && r.value[0]) ? strtoll(r.value, NULL, 10) : fallback;
    free(r.value);
    return PICOMESH_OK(picomesh_int64, v);
}

/* Atomic counter / id bump — OK value is the value after the add. A backend
 * failure is propagated, never collapsed into a (valid) job id 0 / 0 count. */
static struct picomesh_int64_result gp_incr(struct gp_storage *h, struct yheaders *hdrs, const char *key, int64_t delta)
{
    struct picomesh_int64_result r = sharded_storage_db_incr(&h->c, h->obj, hdrs, PIPE_CTX, key, delta);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "git_pipeline: counter update failed", r);
    return r;
}

/* Atomic compare-and-set on a job row. OK value: 1 = swapped, 0 = compare
 * mismatch (another caller changed the row first). A backend error is
 * propagated — distinct from a clean mismatch. */
static struct picomesh_int_result gp_cas(struct gp_storage *h, struct yheaders *hdrs, const char *key,
                                         const char *expected, const char *replacement)
{
    struct picomesh_int_result r =
        sharded_storage_db_compare_and_set(&h->c, h->obj, hdrs, PIPE_CTX, key, expected, replacement);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int, "git_pipeline: compare-and-set failed", r);
    return r;
}

/* Load job:<id> → repo_id/runner_id/status. OK value: 1 = present (out
 * params filled), 0 = absent. A backend read failure is propagated. */
static struct picomesh_int_result job_load(struct gp_storage *h, struct yheaders *hdrs, uint32_t job_id,
                                           uint32_t *repo_id, uint32_t *runner_id, int *status)
{
    char k[40];
    snprintf(k, sizeof(k), "job:%u", job_id);
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, PIPE_CTX, k);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int, "git_pipeline: job read failed", r);
    if (!r.value || !r.value[0]) { free(r.value); return PICOMESH_OK(picomesh_int, 0); }
    char *s = r.value, *t1 = strchr(s, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2) { free(s); return PICOMESH_OK(picomesh_int, 0); }
    *t1 = *t2 = 0;
    if (repo_id) *repo_id = (uint32_t)strtoul(s, NULL, 10);
    if (runner_id) *runner_id = (uint32_t)strtoul(t1 + 1, NULL, 10);
    if (status) *status = atoi(t2 + 1);
    free(s);
    return PICOMESH_OK(picomesh_int, 1);
}

/* Write the canonical job row; propagates a failed write. */
static struct picomesh_void_result job_store(struct gp_storage *h, struct yheaders *hdrs, uint32_t job_id,
                                             uint32_t repo_id, uint32_t runner_id, int status)
{
    char k[40], v[48];
    snprintf(k, sizeof(k), "job:%u", job_id);
    snprintf(v, sizeof(v), "%u\t%u\t%d", repo_id, runner_id, status);
    struct picomesh_int_result r = sharded_storage_db_set(&h->c, h->obj, hdrs, PIPE_CTX, k, v);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "git_pipeline: job write failed", r);
    return PICOMESH_OK_VOID();
}

PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_enqueue")
struct picomesh_uint32_result git_pipeline_git_pipeline_enqueue_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                           uint32_t repo_id)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_enqueue: storage open failed", sr);
    struct gp_storage h = sr.value;
    struct picomesh_int64_result idr = gp_incr(&h, hdrs, "next_id", 1);
    if (PICOMESH_IS_ERR(idr)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_enqueue: allocate id failed", idr);
    uint32_t id = (uint32_t)idr.value;
    struct picomesh_void_result ws = job_store(&h, hdrs, id, repo_id, 0, 0);
    if (PICOMESH_IS_ERR(ws)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_enqueue: persist job failed", ws);
    struct picomesh_int64_result pinc = gp_incr(&h, hdrs, "pending", 1);
    if (PICOMESH_IS_ERR(pinc)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_enqueue: bump pending failed", pinc);
    return PICOMESH_OK(picomesh_uint32, id);
}

PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_lease")
struct picomesh_uint32_result git_pipeline_git_pipeline_lease_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t runner_id)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_lease: storage open failed", sr);
    struct gp_storage h = sr.value;
    /* FIFO: first queued job id wins. Job ids are 1..next_id, so scan that
     * range (lease is not on the throughput hot path — enqueue is). */
    struct picomesh_int64_result lastr = gp_get(&h, hdrs, "next_id", 0);
    if (PICOMESH_IS_ERR(lastr)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_lease: read next_id failed", lastr);
    int64_t last = lastr.value;
    for (uint32_t id = 1; id <= (uint32_t)last; ++id) {
        uint32_t rp = 0; int st = -1;
        struct picomesh_int_result lr = job_load(&h, hdrs, id, &rp, NULL, &st);
        if (PICOMESH_IS_ERR(lr)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_lease: job load failed", lr);
        if (lr.value == 0 || st != 0) continue;  /* absent or not queued */
        /* Claim the job by CAS-ing its row from queued (runner 0, status 0)
         * to running. If two runners race the same queued job, only one CAS
         * swaps; the loser (clean mismatch) falls through to the next id — no
         * double-lease. A backend CAS error propagates. */
        char k[40], expected[48], replacement[48];
        snprintf(k, sizeof(k), "job:%u", id);
        snprintf(expected, sizeof(expected), "%u\t%u\t%d", rp, 0u, 0);
        snprintf(replacement, sizeof(replacement), "%u\t%u\t%d", rp, runner_id, 1);
        struct picomesh_int_result cas = gp_cas(&h, hdrs, k, expected, replacement);
        if (PICOMESH_IS_ERR(cas)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_lease: claim CAS failed", cas);
        if (cas.value == 0) continue;  /* lost the race for this job */
        struct picomesh_int64_result pdec = gp_incr(&h, hdrs, "pending", -1);
        if (PICOMESH_IS_ERR(pdec)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_lease: pending update failed", pdec);
        struct picomesh_int64_result rinc = gp_incr(&h, hdrs, "running", 1);
        if (PICOMESH_IS_ERR(rinc)) return PICOMESH_ERR(picomesh_uint32, "git_pipeline_lease: running update failed", rinc);
        yinfo("git_pipeline: lease job=%u to runner=%u", id, runner_id);
        return PICOMESH_OK(picomesh_uint32, id);
    }
    return PICOMESH_OK(picomesh_uint32, 0);
}

PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_complete")
struct picomesh_int_result git_pipeline_git_pipeline_complete_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t job_id, int32_t status)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "git_pipeline_complete: storage open failed", sr);
    struct gp_storage h = sr.value;
    uint32_t rp = 0, run = 0; int st = -1;
    struct picomesh_int_result lr = job_load(&h, hdrs, job_id, &rp, &run, &st);
    if (PICOMESH_IS_ERR(lr)) return PICOMESH_ERR(picomesh_int, "git_pipeline_complete: load failed", lr);
    if (lr.value == 0) return PICOMESH_OK(picomesh_int, 0);          /* unknown job */
    if (st == 2 || st == 3) return PICOMESH_OK(picomesh_int, 0);     /* already final */
    int final_status = status == 0 ? 2 : 3;
    /* Transition the row atomically; only the caller that swaps adjusts the
     * counters, so a double-complete can't double-count. A clean CAS mismatch
     * (value 0) is a concurrent change; a backend error propagates. */
    char k[40], expected[48], replacement[48];
    snprintf(k, sizeof(k), "job:%u", job_id);
    snprintf(expected, sizeof(expected), "%u\t%u\t%d", rp, run, st);
    snprintf(replacement, sizeof(replacement), "%u\t%u\t%d", rp, run, final_status);
    struct picomesh_int_result cas = gp_cas(&h, hdrs, k, expected, replacement);
    if (PICOMESH_IS_ERR(cas)) return PICOMESH_ERR(picomesh_int, "git_pipeline_complete: transition CAS failed", cas);
    if (cas.value == 0) return PICOMESH_OK(picomesh_int, 0);
    if (st == 0) {
        struct picomesh_int64_result d = gp_incr(&h, hdrs, "pending", -1);
        if (PICOMESH_IS_ERR(d)) return PICOMESH_ERR(picomesh_int, "git_pipeline_complete: pending update failed", d);
    } else if (st == 1) {
        struct picomesh_int64_result d = gp_incr(&h, hdrs, "running", -1);
        if (PICOMESH_IS_ERR(d)) return PICOMESH_ERR(picomesh_int, "git_pipeline_complete: running update failed", d);
    }
    struct picomesh_int64_result dn = gp_incr(&h, hdrs, "done", 1);
    if (PICOMESH_IS_ERR(dn)) return PICOMESH_ERR(picomesh_int, "git_pipeline_complete: done update failed", dn);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_count_pending")
struct picomesh_size_result git_pipeline_git_pipeline_count_pending_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "git_pipeline_count_pending: storage open failed", sr);
    struct gp_storage h = sr.value;
    struct picomesh_int64_result nr = gp_get(&h, hdrs, "pending", 0);
    if (PICOMESH_IS_ERR(nr)) return PICOMESH_ERR(picomesh_size, "git_pipeline_count_pending: read failed", nr);
    return PICOMESH_OK(picomesh_size, (size_t)(nr.value < 0 ? 0 : nr.value));
}

PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_count_running")
struct picomesh_size_result git_pipeline_git_pipeline_count_running_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "git_pipeline_count_running: storage open failed", sr);
    struct gp_storage h = sr.value;
    struct picomesh_int64_result nr = gp_get(&h, hdrs, "running", 0);
    if (PICOMESH_IS_ERR(nr)) return PICOMESH_ERR(picomesh_size, "git_pipeline_count_running: read failed", nr);
    return PICOMESH_OK(picomesh_size, (size_t)(nr.value < 0 ? 0 : nr.value));
}

PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_count_done")
struct picomesh_size_result git_pipeline_git_pipeline_count_done_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "git_pipeline_count_done: storage open failed", sr);
    struct gp_storage h = sr.value;
    struct picomesh_int64_result nr = gp_get(&h, hdrs, "done", 0);
    if (PICOMESH_IS_ERR(nr)) return PICOMESH_ERR(picomesh_size, "git_pipeline_count_done: read failed", nr);
    return PICOMESH_OK(picomesh_size, (size_t)(nr.value < 0 ? 0 : nr.value));
}

/* List ALL pipeline runs' stored entries as a JSON array (gh#15) — every
 * object, not a pending/running/done count. Delegates to the namespace scan. */
PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_list")
struct picomesh_json_result git_pipeline_git_pipeline_list_impl(struct ctx *ctx, struct object *obj,
                                                         struct yheaders *hdrs,
                                                         int64_t offset, int64_t limit)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "git_pipeline_list: storage open failed", sr);
    struct gp_storage h = sr.value;
    return sharded_storage_db_list(&h.c, h.obj, hdrs, PIPE_CTX, "job:", offset, limit);
}

/* Unbounded variant — every run. Use with care on large deployments. */
PICOMESH_CLASS_ANNOTATE("override@git_pipeline:git_pipeline:git_pipeline_list_all")
struct picomesh_json_result git_pipeline_git_pipeline_list_all_impl(struct ctx *ctx, struct object *obj,
                                                                    struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct gp_storage_result sr = gp_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "git_pipeline_list_all: storage open failed", sr);
    struct gp_storage h = sr.value;
    return sharded_storage_db_list_all(&h.c, h.obj, hdrs, PIPE_CTX, "job:");
}

#include "store.gen.c"
