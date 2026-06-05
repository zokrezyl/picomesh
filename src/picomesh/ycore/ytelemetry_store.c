/* ytelemetry_store — in-memory trace collector store (see ytelemetry_store.h).
 *
 * Sharded for ingest parallelism: spans are partitioned into YTEL_SHARDS
 * independent rings by a hash of their trace_id, each guarded by its own
 * lock. The collector's worker threads therefore contend only with the
 * ~1/N other threads that happen to hit the same shard, instead of
 * serializing every ingest on one global mutex. Because all spans of a
 * trace share a trace_id they land in the same shard, so a single-trace
 * lookup touches exactly one shard; the multi-trace queries fan out across
 * all shards and merge. */

#include <picomesh/ycore/ytelemetry_store.h>

#include <picomesh/yjson/yjson.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define YTEL_STORE_DEFAULT_MAX 50000
#define YTEL_SHARDS 16u /* must be a power of two */

struct ytelemetry_stored_span {
    char trace_id[33];
    char span_id[17];
    char parent_id[17];
    char name[64];
    char service[64];
    char node[80];
    char status[8]; /* "ok" / "error" */
    char kind[12];
    char err[96];
    uint64_t start_unix_ns;
    uint64_t duration_ns;
    uint32_t uid;
    int used;
};

struct ytel_shard {
    pthread_mutex_t mu;
    struct ytelemetry_stored_span *ring;
    size_t cap;
    size_t head;  /* next write slot */
    size_t count; /* live entries (<= cap) */
    uint64_t ingested;
    uint64_t malformed;
    uint64_t evicted;
};

struct ytelemetry_store {
    struct ytel_shard shards[YTEL_SHARDS];
    size_t shard_cap; /* per-shard capacity (total / YTEL_SHARDS) */
    uint64_t max_age_ns;
    int inited;
    pthread_mutex_t init_mu;
};

static struct ytelemetry_store *ytelemetry_store_state(void)
{
    static struct ytelemetry_store s = {
        .shards = {[0 ... YTEL_SHARDS - 1] = {.mu = PTHREAD_MUTEX_INITIALIZER}},
        .init_mu = PTHREAD_MUTEX_INITIALIZER,
    };
    return &s;
}

static uint64_t ytelemetry_store_now_ns(void)
{
    struct timespec ts = {0};
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void ytelemetry_store_copystr(char *dst, size_t cap, const char *src)
{
    if (!cap) return;
    size_t i = 0;
    if (src) for (; src[i] && i < cap - 1; ++i) dst[i] = src[i];
    dst[i] = 0;
}

/* FNV-1a over the trace_id, masked to a shard index. */
static unsigned ytelemetry_shard_for(const char *trace_id)
{
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char *p = (const unsigned char *)trace_id; p && *p; ++p) {
        hash ^= *p;
        hash *= 1099511628211ull;
    }
    return (unsigned)(hash & (YTEL_SHARDS - 1));
}

/* Establish per-shard capacity once. Callers that ingest/query first hit this;
 * ytelemetry_store_init may run earlier to override the totals. */
static void ytelemetry_store_ensure(struct ytelemetry_store *s)
{
    if (s->inited) return;
    pthread_mutex_lock(&s->init_mu);
    if (!s->inited) {
        if (!s->shard_cap) s->shard_cap = YTEL_STORE_DEFAULT_MAX / YTEL_SHARDS;
        if (!s->shard_cap) s->shard_cap = 1;
        s->inited = 1;
    }
    pthread_mutex_unlock(&s->init_mu);
}

/* Lazily allocate a shard's ring. Caller holds the shard lock. */
static void ytelemetry_shard_ensure_ring(struct ytel_shard *sh, size_t cap)
{
    if (sh->ring) return;
    sh->cap = cap ? cap : 1;
    sh->ring = calloc(sh->cap, sizeof(*sh->ring));
    if (!sh->ring) sh->cap = 0;
}

void ytelemetry_store_init(size_t max_spans, uint64_t max_age_seconds)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->init_mu);
    if (!s->inited) {
        size_t total = max_spans ? max_spans : YTEL_STORE_DEFAULT_MAX;
        s->shard_cap = total / YTEL_SHARDS;
        if (!s->shard_cap) s->shard_cap = 1;
        s->max_age_ns = max_age_seconds * 1000000000ull;
        s->inited = 1;
    }
    pthread_mutex_unlock(&s->init_mu);
}

/* The i-th newest live span in a shard (0 = newest). Caller holds shard lock. */
static struct ytelemetry_stored_span *ytelemetry_shard_nth(struct ytel_shard *sh, size_t i)
{
    if (i >= sh->count) return NULL;
    size_t idx = (sh->head + sh->cap - 1 - i) % sh->cap;
    return &sh->ring[idx];
}

static int ytelemetry_store_fresh(uint64_t max_age_ns,
                                  const struct ytelemetry_stored_span *sp, uint64_t now)
{
    if (!max_age_ns) return 1;
    return sp->start_unix_ns + max_age_ns >= now;
}

/* Copy a parsed span into a stored_span (all strings interned, so the source
 * yjson_doc can be freed immediately afterwards). */
static void ytelemetry_fill_span(struct ytelemetry_stored_span *dst, const struct yjson_value *root,
                                 const char *trace, const char *span)
{
    memset(dst, 0, sizeof(*dst));
    ytelemetry_store_copystr(dst->trace_id, sizeof(dst->trace_id), trace);
    ytelemetry_store_copystr(dst->span_id, sizeof(dst->span_id), span);
    ytelemetry_store_copystr(dst->parent_id, sizeof(dst->parent_id),
                       yjson_as_string(yjson_object_get(root, "parent_span_id"), ""));
    ytelemetry_store_copystr(dst->name, sizeof(dst->name),
                       yjson_as_string(yjson_object_get(root, "name"), ""));
    ytelemetry_store_copystr(dst->service, sizeof(dst->service),
                       yjson_as_string(yjson_object_get(root, "service_name"), ""));
    ytelemetry_store_copystr(dst->node, sizeof(dst->node),
                       yjson_as_string(yjson_object_get(root, "node_id"), ""));
    ytelemetry_store_copystr(dst->status, sizeof(dst->status),
                       yjson_as_string(yjson_object_get(root, "status"), "ok"));
    ytelemetry_store_copystr(dst->kind, sizeof(dst->kind),
                       yjson_as_string(yjson_object_get(root, "kind"), "internal"));
    ytelemetry_store_copystr(dst->err, sizeof(dst->err),
                       yjson_as_string(yjson_object_get(root, "error_message"), ""));
    dst->start_unix_ns = (uint64_t)yjson_as_int(yjson_object_get(root, "start_time_ns"), 0);
    dst->duration_ns = (uint64_t)yjson_as_int(yjson_object_get(root, "duration_ns"), 0);
    const struct yjson_value *attrs = yjson_object_get(root, "attributes");
    if (attrs)
        dst->uid = (uint32_t)yjson_as_int(yjson_object_get(attrs, "picomesh.uid"), 0);
    dst->used = 1;
}

/* Append one already-filled span into a shard's ring. Caller holds sh->mu. */
static void ytelemetry_shard_put(struct ytel_shard *sh, size_t cap,
                                 const struct ytelemetry_stored_span *src)
{
    ytelemetry_shard_ensure_ring(sh, cap);
    if (!sh->ring) { sh->malformed++; return; }
    if (sh->count == sh->cap) sh->evicted++; /* overwriting the oldest live span */
    sh->ring[sh->head] = *src;
    sh->head = (sh->head + 1) % sh->cap;
    if (sh->count < sh->cap) sh->count++;
    sh->ingested++;
}

/* Per-worker, thread-confined accumulation arena. Worker coroutines are
 * cooperative within a thread, so the hot-path append takes NO lock; the shard
 * lock is acquired only when a per-shard bucket fills (≈1 lock per BUCKET
 * spans) or on the periodic time flush. Spans are pre-bucketed by destination
 * shard, so a flush is a single bulk copy under one shard lock — there is no
 * per-span locking anywhere on the path. */
#define YTEL_ARENA_BUCKET 256
#define YTEL_FLUSH_NS 50000000ull /* 50ms: bounds query staleness for a busy worker */

struct ytel_arena {
    struct ytelemetry_stored_span bucket[YTEL_SHARDS][YTEL_ARENA_BUCKET];
    size_t fill[YTEL_SHARDS];
    size_t total;
    uint64_t last_flush_ns;
};

static __thread struct ytel_arena *ytel_tls_arena;

static void ytelemetry_arena_flush_shard(struct ytelemetry_store *s, struct ytel_arena *a, unsigned k)
{
    if (!a->fill[k]) return;
    struct ytel_shard *sh = &s->shards[k];
    pthread_mutex_lock(&sh->mu);
    for (size_t i = 0; i < a->fill[k]; ++i)
        ytelemetry_shard_put(sh, s->shard_cap, &a->bucket[k][i]);
    pthread_mutex_unlock(&sh->mu);
    a->total -= a->fill[k];
    a->fill[k] = 0;
}

static void ytelemetry_arena_flush_all(struct ytelemetry_store *s, struct ytel_arena *a, uint64_t now)
{
    for (unsigned k = 0; k < YTEL_SHARDS; ++k) ytelemetry_arena_flush_shard(s, a, k);
    a->last_flush_ns = now;
}

void ytelemetry_store_flush_local(void)
{
    struct ytel_arena *a = ytel_tls_arena;
    if (!a || !a->total) return;
    ytelemetry_arena_flush_all(ytelemetry_store_state(), a, ytelemetry_store_now_ns());
}

int ytelemetry_store_ingest_json(const char *json, size_t len)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);

    struct yjson_doc *doc = yjson_parse(json, len);
    const struct yjson_value *root = doc ? yjson_doc_root(doc) : NULL;
    const char *trace = root ? yjson_as_string(yjson_object_get(root, "trace_id"), NULL) : NULL;
    const char *span = root ? yjson_as_string(yjson_object_get(root, "span_id"), NULL) : NULL;
    if (!trace || !span) {
        /* Charge the malformed span to a deterministic shard so the tally is
         * still visible without a global counter. */
        struct ytel_shard *sh = &s->shards[0];
        pthread_mutex_lock(&sh->mu); sh->malformed++; pthread_mutex_unlock(&sh->mu);
        if (doc) yjson_doc_free(doc);
        return 0;
    }

    unsigned k = ytelemetry_shard_for(trace);
    struct ytel_arena *a = ytel_tls_arena;
    if (!a) {
        a = calloc(1, sizeof(*a));
        if (!a) {
            /* No arena (allocation failed): fall back to a direct locked put. */
            struct ytelemetry_stored_span one;
            ytelemetry_fill_span(&one, root, trace, span);
            struct ytel_shard *sh = &s->shards[k];
            pthread_mutex_lock(&sh->mu);
            ytelemetry_shard_put(sh, s->shard_cap, &one);
            pthread_mutex_unlock(&sh->mu);
            yjson_doc_free(doc);
            return 1;
        }
        ytel_tls_arena = a;
    }

    /* Lock-free hot path: fill the parsed span into this worker's own bucket. */
    ytelemetry_fill_span(&a->bucket[k][a->fill[k]], root, trace, span);
    a->fill[k]++;
    a->total++;
    yjson_doc_free(doc);

    if (a->fill[k] == YTEL_ARENA_BUCKET) {
        /* Bucket full → bulk-flush just this shard (one lock for BUCKET spans). */
        ytelemetry_arena_flush_shard(s, a, k);
    } else if ((a->total & 63) == 0) {
        /* Periodically bound staleness without a clock call per span. */
        uint64_t now = ytelemetry_store_now_ns();
        if (now - a->last_flush_ns > YTEL_FLUSH_NS) ytelemetry_arena_flush_all(s, a, now);
    }
    return 1;
}

/* {ingested, malformed, evicted, stored, capacity, max_age_seconds} */
void ytelemetry_store_write_stats(struct yjson_writer *w)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    uint64_t ingested = 0, malformed = 0, evicted = 0, stored = 0;
    for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
        struct ytel_shard *sh = &s->shards[k];
        pthread_mutex_lock(&sh->mu);
        ingested += sh->ingested;
        malformed += sh->malformed;
        evicted += sh->evicted;
        stored += sh->count;
        pthread_mutex_unlock(&sh->mu);
    }
    yjson_writer_begin_object(w);
    yjson_writer_key(w, "ingested"); yjson_writer_int(w, (int64_t)ingested);
    yjson_writer_key(w, "malformed"); yjson_writer_int(w, (int64_t)malformed);
    yjson_writer_key(w, "evicted"); yjson_writer_int(w, (int64_t)evicted);
    yjson_writer_key(w, "stored"); yjson_writer_int(w, (int64_t)stored);
    yjson_writer_key(w, "capacity"); yjson_writer_int(w, (int64_t)(s->shard_cap * YTEL_SHARDS));
    yjson_writer_key(w, "max_age_seconds"); yjson_writer_int(w, (int64_t)(s->max_age_ns / 1000000000ull));
    yjson_writer_end_object(w);
}

/* ---- query writers ---------------------------------------------------- */

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t pctl_u64(const uint64_t *sorted, size_t n, double p)
{
    if (!n) return 0;
    size_t idx = (size_t)(p / 100.0 * (double)(n - 1) + 0.5);
    if (idx >= n) idx = n - 1;
    return sorted[idx];
}

static void emit_span(struct yjson_writer *w, const struct ytelemetry_stored_span *sp)
{
    yjson_writer_begin_object(w);
    yjson_writer_key(w, "span_id"); yjson_writer_string(w, sp->span_id);
    yjson_writer_key(w, "parent_span_id"); yjson_writer_string(w, sp->parent_id);
    yjson_writer_key(w, "trace_id"); yjson_writer_string(w, sp->trace_id);
    yjson_writer_key(w, "name"); yjson_writer_string(w, sp->name);
    yjson_writer_key(w, "kind"); yjson_writer_string(w, sp->kind);
    yjson_writer_key(w, "service_name"); yjson_writer_string(w, sp->service);
    yjson_writer_key(w, "node_id"); yjson_writer_string(w, sp->node);
    yjson_writer_key(w, "start_time_ns"); yjson_writer_int(w, (int64_t)sp->start_unix_ns);
    yjson_writer_key(w, "duration_ns"); yjson_writer_int(w, (int64_t)sp->duration_ns);
    yjson_writer_key(w, "status"); yjson_writer_string(w, sp->status);
    if (sp->err[0]) { yjson_writer_key(w, "error_message"); yjson_writer_string(w, sp->err); }
    yjson_writer_key(w, "attributes");
    yjson_writer_begin_object(w);
    yjson_writer_key(w, "picomesh.uid"); yjson_writer_int(w, (int64_t)sp->uid);
    yjson_writer_end_object(w);
    yjson_writer_end_object(w);
}

/* {trace_id, root_span_id, duration_ns, span_count, spans:[...]} — single
 * shard: every span of a trace hashes to the same shard. */
void ytelemetry_store_write_trace(struct yjson_writer *w, const char *trace_id)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    struct ytel_shard *sh = &s->shards[ytelemetry_shard_for(trace_id ? trace_id : "")];
    pthread_mutex_lock(&sh->mu);

    enum { CAP = 8192 };
    struct ytelemetry_stored_span *match[CAP];
    size_t n = 0;
    for (size_t i = 0; i < sh->count && n < CAP; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, sh->count - 1 - i);
        if (sp->used && trace_id && strcmp(sp->trace_id, trace_id) == 0)
            match[n++] = sp;
    }

    uint64_t min_start = 0, max_end = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t end = match[i]->start_unix_ns + match[i]->duration_ns;
        if (i == 0 || match[i]->start_unix_ns < min_start) min_start = match[i]->start_unix_ns;
        if (i == 0 || end > max_end) max_end = end;
    }

    const char *root_span = "";
    for (size_t i = 0; i < n && !root_span[0]; ++i)
        if (!match[i]->parent_id[0]) root_span = match[i]->span_id;
    for (size_t i = 0; i < n && !root_span[0]; ++i) {
        int parent_in = 0;
        for (size_t j = 0; j < n; ++j)
            if (strcmp(match[j]->span_id, match[i]->parent_id) == 0) { parent_in = 1; break; }
        if (!parent_in) root_span = match[i]->span_id;
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "trace_id"); yjson_writer_string(w, trace_id ? trace_id : "");
    yjson_writer_key(w, "root_span_id"); yjson_writer_string(w, root_span);
    yjson_writer_key(w, "span_count"); yjson_writer_int(w, (int64_t)n);
    yjson_writer_key(w, "duration_ns"); yjson_writer_int(w, (int64_t)(n ? max_end - min_start : 0));
    yjson_writer_key(w, "spans");
    yjson_writer_begin_array(w);
    for (size_t i = 0; i < n; ++i) emit_span(w, match[i]);
    yjson_writer_end_array(w);
    yjson_writer_end_object(w);

    pthread_mutex_unlock(&sh->mu);
}

/* One summarised trace, copied out so the shard lock can be released before
 * the cross-shard merge. */
struct ytel_trace_sum {
    char trace_id[33];
    char root_name[64];
    char root_service[64];
    uint64_t start;
    uint64_t end;
    size_t span_count;
    int error;
};

static int cmp_sum_newest(const void *a, const void *b)
{
    const struct ytel_trace_sum *x = a, *y = b;
    return (x->start < y->start) - (x->start > y->start); /* newest first */
}

void ytelemetry_store_write_traces(struct yjson_writer *w, const char *service,
                             uint64_t since_ns, const char *status)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    uint64_t now = ytelemetry_store_now_ns();

    enum { PER_SHARD = 64, TOTAL = (int)(YTEL_SHARDS) * PER_SHARD };
    struct ytel_trace_sum *cand = malloc((size_t)TOTAL * sizeof(*cand));
    size_t ncand = 0;

    if (cand) {
        for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
            struct ytel_shard *sh = &s->shards[k];
            pthread_mutex_lock(&sh->mu);

            char seen[PER_SHARD][33];
            size_t nseen = 0;
            for (size_t i = 0; i < sh->count && nseen < PER_SHARD; ++i) {
                struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i); /* newest→oldest */
                if (!sp->used || !ytelemetry_store_fresh(s->max_age_ns, sp, now)) continue;
                if (since_ns && sp->start_unix_ns < since_ns) continue;
                if (service && service[0] && strcmp(sp->service, service) != 0) continue;

                int dup = 0;
                for (size_t j = 0; j < nseen; ++j)
                    if (strcmp(seen[j], sp->trace_id) == 0) { dup = 1; break; }
                if (dup) continue;

                /* Summarise the whole trace — all its spans are in this shard. */
                struct ytel_trace_sum sum = {0};
                ytelemetry_store_copystr(sum.trace_id, sizeof(sum.trace_id), sp->trace_id);
                ytelemetry_store_copystr(sum.root_name, sizeof(sum.root_name), sp->name);
                ytelemetry_store_copystr(sum.root_service, sizeof(sum.root_service), sp->service);
                sum.start = sp->start_unix_ns;
                sum.end = sp->start_unix_ns + sp->duration_ns;
                for (size_t m = 0; m < sh->count; ++m) {
                    struct ytelemetry_stored_span *o = ytelemetry_shard_nth(sh, m);
                    if (!o->used || strcmp(o->trace_id, sp->trace_id) != 0) continue;
                    sum.span_count++;
                    if (o->start_unix_ns < sum.start) sum.start = o->start_unix_ns;
                    uint64_t e = o->start_unix_ns + o->duration_ns;
                    if (e > sum.end) sum.end = e;
                    if (strcmp(o->status, "error") == 0) sum.error = 1;
                    if (!o->parent_id[0]) {
                        ytelemetry_store_copystr(sum.root_name, sizeof(sum.root_name), o->name);
                        ytelemetry_store_copystr(sum.root_service, sizeof(sum.root_service), o->service);
                    }
                }

                ytelemetry_store_copystr(seen[nseen], sizeof(seen[nseen]), sp->trace_id);
                nseen++;
                cand[ncand++] = sum;
            }
            pthread_mutex_unlock(&sh->mu);
        }
        qsort(cand, ncand, sizeof(*cand), cmp_sum_newest);
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "traces");
    yjson_writer_begin_array(w);
    size_t emitted = 0;
    for (size_t i = 0; i < ncand && emitted < 256; ++i) {
        const char *tstatus = cand[i].error ? "error" : "ok";
        if (status && status[0] && strcmp(status, tstatus) != 0) continue;
        yjson_writer_begin_object(w);
        yjson_writer_key(w, "trace_id"); yjson_writer_string(w, cand[i].trace_id);
        yjson_writer_key(w, "root_name"); yjson_writer_string(w, cand[i].root_name);
        yjson_writer_key(w, "service_name"); yjson_writer_string(w, cand[i].root_service);
        yjson_writer_key(w, "start_time_ns"); yjson_writer_int(w, (int64_t)cand[i].start);
        yjson_writer_key(w, "duration_ns"); yjson_writer_int(w, (int64_t)(cand[i].end - cand[i].start));
        yjson_writer_key(w, "span_count"); yjson_writer_int(w, (int64_t)cand[i].span_count);
        yjson_writer_key(w, "status"); yjson_writer_string(w, tstatus);
        yjson_writer_end_object(w);
        emitted++;
    }
    yjson_writer_end_array(w);
    yjson_writer_end_object(w);
    free(cand);
}

void ytelemetry_store_write_services(struct yjson_writer *w)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);

    enum { MAX_SVC = 128 };
    char names[MAX_SVC][64];
    size_t counts[MAX_SVC] = {0};
    size_t n = 0;
    for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
        struct ytel_shard *sh = &s->shards[k];
        pthread_mutex_lock(&sh->mu);
        for (size_t i = 0; i < sh->count; ++i) {
            struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i);
            if (!sp->used || !sp->service[0]) continue;
            size_t j = 0;
            for (; j < n; ++j) if (strcmp(names[j], sp->service) == 0) break;
            if (j == n && n < MAX_SVC) { ytelemetry_store_copystr(names[n], 64, sp->service); n++; }
            if (j < MAX_SVC) counts[j]++;
        }
        pthread_mutex_unlock(&sh->mu);
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "services");
    yjson_writer_begin_array(w);
    for (size_t j = 0; j < n; ++j) {
        yjson_writer_begin_object(w);
        yjson_writer_key(w, "service_name"); yjson_writer_string(w, names[j]);
        yjson_writer_key(w, "span_count"); yjson_writer_int(w, (int64_t)counts[j]);
        yjson_writer_end_object(w);
    }
    yjson_writer_end_array(w);
    yjson_writer_end_object(w);
}

void ytelemetry_store_write_operations(struct yjson_writer *w, const char *service)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);

    enum { MAX_OPS = 256 };
    char ops[MAX_OPS][64];
    size_t counts[MAX_OPS] = {0};
    size_t n = 0;
    for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
        struct ytel_shard *sh = &s->shards[k];
        pthread_mutex_lock(&sh->mu);
        for (size_t i = 0; i < sh->count; ++i) {
            struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i);
            if (!sp->used || !sp->name[0]) continue;
            if (service && service[0] && strcmp(sp->service, service) != 0) continue;
            size_t j = 0;
            for (; j < n; ++j) if (strcmp(ops[j], sp->name) == 0) break;
            if (j == n && n < MAX_OPS) { ytelemetry_store_copystr(ops[n], 64, sp->name); n++; }
            if (j < MAX_OPS) counts[j]++;
        }
        pthread_mutex_unlock(&sh->mu);
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "service"); yjson_writer_string(w, service ? service : "");
    yjson_writer_key(w, "operations");
    yjson_writer_begin_array(w);
    for (size_t j = 0; j < n; ++j) {
        yjson_writer_begin_object(w);
        yjson_writer_key(w, "name"); yjson_writer_string(w, ops[j]);
        yjson_writer_key(w, "count"); yjson_writer_int(w, (int64_t)counts[j]);
        yjson_writer_end_object(w);
    }
    yjson_writer_end_array(w);
    yjson_writer_end_object(w);
}

void ytelemetry_store_write_latency(struct yjson_writer *w, const char *service,
                             const char *operation, uint64_t window_ns)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    uint64_t now = ytelemetry_store_now_ns();
    uint64_t floor = window_ns && now > window_ns ? now - window_ns : 0;

    size_t total = 0;
    for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
        pthread_mutex_lock(&s->shards[k].mu);
        total += s->shards[k].count;
        pthread_mutex_unlock(&s->shards[k].mu);
    }

    uint64_t *durs = malloc((total ? total : 1) * sizeof(uint64_t));
    size_t n = 0;
    if (durs) {
        for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
            struct ytel_shard *sh = &s->shards[k];
            pthread_mutex_lock(&sh->mu);
            for (size_t i = 0; i < sh->count && n < total; ++i) {
                struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i);
                if (!sp->used) continue;
                if (floor && sp->start_unix_ns < floor) continue;
                if (service && service[0] && strcmp(sp->service, service) != 0) continue;
                if (operation && operation[0] && strcmp(sp->name, operation) != 0) continue;
                durs[n++] = sp->duration_ns;
            }
            pthread_mutex_unlock(&sh->mu);
        }
        qsort(durs, n, sizeof(uint64_t), cmp_u64);
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "service"); yjson_writer_string(w, service ? service : "");
    yjson_writer_key(w, "operation"); yjson_writer_string(w, operation ? operation : "");
    yjson_writer_key(w, "window_ns"); yjson_writer_int(w, (int64_t)window_ns);
    yjson_writer_key(w, "count"); yjson_writer_int(w, (int64_t)n);
    yjson_writer_key(w, "p50_ns"); yjson_writer_int(w, (int64_t)pctl_u64(durs, n, 50));
    yjson_writer_key(w, "p90_ns"); yjson_writer_int(w, (int64_t)pctl_u64(durs, n, 90));
    yjson_writer_key(w, "p99_ns"); yjson_writer_int(w, (int64_t)pctl_u64(durs, n, 99));
    yjson_writer_key(w, "max_ns"); yjson_writer_int(w, (int64_t)(n ? durs[n - 1] : 0));
    yjson_writer_end_object(w);

    free(durs);
}

void ytelemetry_store_write_errors(struct yjson_writer *w, uint64_t since_ns)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);

    enum { PER_SHARD = 64, TOTAL = (int)(YTEL_SHARDS) * PER_SHARD };
    struct ytelemetry_stored_span *errs = malloc((size_t)TOTAL * sizeof(*errs));
    size_t nerr = 0;
    if (errs) {
        for (unsigned k = 0; k < YTEL_SHARDS; ++k) {
            struct ytel_shard *sh = &s->shards[k];
            pthread_mutex_lock(&sh->mu);
            size_t taken = 0;
            for (size_t i = 0; i < sh->count && taken < PER_SHARD; ++i) {
                struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i); /* newest first */
                if (!sp->used || strcmp(sp->status, "error") != 0) continue;
                if (since_ns && sp->start_unix_ns < since_ns) continue;
                errs[nerr++] = *sp; /* copy out before unlocking */
                taken++;
            }
            pthread_mutex_unlock(&sh->mu);
        }
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "errors");
    yjson_writer_begin_array(w);
    if (errs) {
        /* newest first across shards: sort by start descending */
        for (size_t a = 0; a + 1 < nerr; ++a)
            for (size_t b = a + 1; b < nerr; ++b)
                if (errs[b].start_unix_ns > errs[a].start_unix_ns) {
                    struct ytelemetry_stored_span tmp = errs[a];
                    errs[a] = errs[b];
                    errs[b] = tmp;
                }
        size_t emitted = 0;
        for (size_t i = 0; i < nerr && emitted < 256; ++i) { emit_span(w, &errs[i]); emitted++; }
    }
    yjson_writer_end_array(w);
    yjson_writer_end_object(w);
    free(errs);
}
