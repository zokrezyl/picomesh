/* ytelemetry_store — in-memory trace collector store (see ytelemetry_store.h).
 *
 * Sharded for ingest parallelism, with a per-worker lock-free arena in front:
 *
 *   - Each worker thread accumulates parsed spans in a thread-confined arena,
 *     pre-bucketed by destination shard. The hot path takes NO lock (worker
 *     coroutines are cooperative, so the arena has a single writer).
 *   - A shard lock is taken only to FLUSH a full bucket (≈1 lock per
 *     `bucket_spans` spans) or on the periodic time flush — never per span.
 *   - The shared store is `shards` independent rings, each its own lock; a
 *     span's shard is hash(trace_id) % shards, so all spans of a trace stay
 *     co-located (single-shard get_trace; multi-trace queries fan out + merge).
 *
 * Every size/cadence knob is configurable via ytelemetry_store_init_config. */

#include <picomesh/ycore/ytelemetry_store.h>

#include <picomesh/yjson/yjson.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <uthash.h>

#define YTEL_DEFAULT_MAX_SPANS 50000
#define YTEL_DEFAULT_SHARDS 16u
#define YTEL_DEFAULT_BUCKET_SPANS 256
#define YTEL_DEFAULT_FLUSH_MS 50

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

/* Incrementally-maintained summary of one trace's live spans in a shard, so
 * the list query never rescans the ring. Created/updated as spans are stored,
 * decremented as they are evicted, removed when the trace's last span ages out.
 * Keyed by trace_id in the shard's `index` hash. */
struct ytel_trace_idx {
    char trace_id[33]; /* hash key */
    char root_name[64];
    char root_service[64];
    uint64_t start;
    uint64_t end;
    uint32_t live; /* non-evicted spans of this trace currently in the ring */
    int error;
    UT_hash_handle hh;
};

struct ytel_shard {
    pthread_mutex_t mu;
    struct ytelemetry_stored_span *ring;
    struct ytel_trace_idx *index; /* trace_id -> live summary */
    size_t cap;
    size_t head;  /* next write slot */
    size_t count; /* live entries (<= cap) */
    uint64_t ingested;
    uint64_t malformed;
    uint64_t evicted;
};

struct ytelemetry_store {
    struct ytel_shard *shards; /* [shard_count], allocated at init */
    unsigned shard_count;
    size_t shard_cap;    /* per-shard ring capacity */
    size_t bucket_spans; /* per-shard arena bucket size */
    uint64_t max_age_ns;
    uint64_t flush_ns;
    int inited;
    pthread_mutex_t init_mu;
};

static struct ytelemetry_store *ytelemetry_store_state(void)
{
    static struct ytelemetry_store s = {.init_mu = PTHREAD_MUTEX_INITIALIZER};
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

/* FNV-1a over the trace_id, reduced to a shard index. */
static unsigned ytelemetry_shard_for(const char *trace_id, unsigned shard_count)
{
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char *p = (const unsigned char *)trace_id; p && *p; ++p) {
        hash ^= *p;
        hash *= 1099511628211ull;
    }
    return (unsigned)(hash % shard_count);
}

/* Apply config + allocate shards. Caller holds init_mu; first call wins. */
static void ytelemetry_store_setup_locked(struct ytelemetry_store *s,
                                          const struct ytelemetry_store_config *config)
{
    if (s->inited) return;

    struct ytelemetry_store_config cfg = config ? *config : (struct ytelemetry_store_config){0};
    size_t total = cfg.max_spans ? cfg.max_spans : YTEL_DEFAULT_MAX_SPANS;
    s->shard_count = cfg.shards ? cfg.shards : YTEL_DEFAULT_SHARDS;
    s->bucket_spans = cfg.bucket_spans ? cfg.bucket_spans : YTEL_DEFAULT_BUCKET_SPANS;
    s->flush_ns = (cfg.flush_ms ? cfg.flush_ms : YTEL_DEFAULT_FLUSH_MS) * 1000000ull;
    s->max_age_ns = cfg.max_age_seconds * 1000000000ull;

    s->shards = calloc(s->shard_count, sizeof(*s->shards));
    if (!s->shards) { s->shard_count = 1; s->shards = calloc(1, sizeof(*s->shards)); }
    s->shard_cap = total / s->shard_count;
    if (!s->shard_cap) s->shard_cap = 1;
    for (unsigned k = 0; k < s->shard_count; ++k)
        pthread_mutex_init(&s->shards[k].mu, NULL);

    s->inited = 1;
}

static void ytelemetry_store_ensure(struct ytelemetry_store *s)
{
    if (s->inited) return;
    pthread_mutex_lock(&s->init_mu);
    ytelemetry_store_setup_locked(s, NULL);
    pthread_mutex_unlock(&s->init_mu);
}

void ytelemetry_store_init_config(const struct ytelemetry_store_config *config)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->init_mu);
    ytelemetry_store_setup_locked(s, config);
    pthread_mutex_unlock(&s->init_mu);
}

void ytelemetry_store_init(size_t max_spans, uint64_t max_age_seconds)
{
    struct ytelemetry_store_config cfg = {.max_spans = max_spans, .max_age_seconds = max_age_seconds};
    ytelemetry_store_init_config(&cfg);
}

/* Lazily allocate a shard's ring. Caller holds the shard lock. */
static void ytelemetry_shard_ensure_ring(struct ytel_shard *sh, size_t cap)
{
    if (sh->ring) return;
    sh->cap = cap ? cap : 1;
    sh->ring = calloc(sh->cap, sizeof(*sh->ring));
    if (!sh->ring) sh->cap = 0;
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
    if (sh->count == sh->cap) {
        /* Evicting ring[head] — drop it from its trace's index entry. */
        struct ytelemetry_stored_span *old = &sh->ring[sh->head];
        if (old->used) {
            struct ytel_trace_idx *e = NULL;
            HASH_FIND_STR(sh->index, old->trace_id, e);
            if (e && e->live && --e->live == 0) { HASH_DEL(sh->index, e); free(e); }
        }
        sh->evicted++;
    }
    sh->ring[sh->head] = *src;
    sh->head = (sh->head + 1) % sh->cap;
    if (sh->count < sh->cap) sh->count++;
    sh->ingested++;

    /* Maintain the per-trace summary so list/detail queries are O(1) lookups. */
    struct ytel_trace_idx *e = NULL;
    HASH_FIND_STR(sh->index, src->trace_id, e);
    if (!e) {
        e = calloc(1, sizeof(*e));
        if (e) {
            ytelemetry_store_copystr(e->trace_id, sizeof(e->trace_id), src->trace_id);
            e->start = src->start_unix_ns;
            e->end = src->start_unix_ns + src->duration_ns;
            HASH_ADD_STR(sh->index, trace_id, e);
        }
    }
    if (e) {
        e->live++;
        if (src->start_unix_ns < e->start) e->start = src->start_unix_ns;
        uint64_t end = src->start_unix_ns + src->duration_ns;
        if (end > e->end) e->end = end;
        if (strcmp(src->status, "error") == 0) e->error = 1;
        if (!e->root_name[0] || !src->parent_id[0]) { /* first span, or the real root */
            ytelemetry_store_copystr(e->root_name, sizeof(e->root_name), src->name);
            ytelemetry_store_copystr(e->root_service, sizeof(e->root_service), src->service);
        }
    }
}

/* Per-worker, thread-confined accumulation arena (see file header). Buckets are
 * a flat [shard_count * bucket_spans] array, sized from the store config the
 * first time this thread ingests. */
struct ytel_arena {
    struct ytelemetry_stored_span *spans; /* shard_count * bucket_spans */
    size_t *fill;                         /* per-shard fill counts */
    unsigned shard_count;
    size_t bucket_spans;
    size_t total;
    uint64_t last_flush_ns;
};

static __thread struct ytel_arena ytel_tls_arena;

static void ytelemetry_arena_flush_shard(struct ytelemetry_store *s, struct ytel_arena *a, unsigned k)
{
    if (!a->fill[k]) return;
    struct ytel_shard *sh = &s->shards[k];
    struct ytelemetry_stored_span *bucket = &a->spans[(size_t)k * a->bucket_spans];
    pthread_mutex_lock(&sh->mu);
    for (size_t i = 0; i < a->fill[k]; ++i)
        ytelemetry_shard_put(sh, s->shard_cap, &bucket[i]);
    pthread_mutex_unlock(&sh->mu);
    a->total -= a->fill[k];
    a->fill[k] = 0;
}

static void ytelemetry_arena_flush_all(struct ytelemetry_store *s, struct ytel_arena *a, uint64_t now)
{
    for (unsigned k = 0; k < a->shard_count; ++k) ytelemetry_arena_flush_shard(s, a, k);
    a->last_flush_ns = now;
}

void ytelemetry_store_flush_local(void)
{
    struct ytel_arena *a = &ytel_tls_arena;
    if (!a->spans || !a->total) return;
    ytelemetry_arena_flush_all(ytelemetry_store_state(), a, ytelemetry_store_now_ns());
}

/* Append one already-parsed span object into the calling thread's arena. The
 * lock-free hot path shared by the single and batch ingest entry points.
 * Returns 1 if accepted, 0 if the span is missing required fields. */
static int ytelemetry_append_parsed(struct ytelemetry_store *s, const struct yjson_value *root)
{
    const char *trace = root ? yjson_as_string(yjson_object_get(root, "trace_id"), NULL) : NULL;
    const char *span = root ? yjson_as_string(yjson_object_get(root, "span_id"), NULL) : NULL;
    if (!trace || !span) {
        struct ytel_shard *sh = &s->shards[0];
        pthread_mutex_lock(&sh->mu); sh->malformed++; pthread_mutex_unlock(&sh->mu);
        return 0;
    }

    unsigned k = ytelemetry_shard_for(trace, s->shard_count);

    struct ytel_arena *a = &ytel_tls_arena;
    if (!a->spans) {
        a->shard_count = s->shard_count;
        a->bucket_spans = s->bucket_spans;
        a->spans = calloc((size_t)a->shard_count * a->bucket_spans, sizeof(*a->spans));
        a->fill = calloc(a->shard_count, sizeof(*a->fill));
        if (!a->spans || !a->fill) {
            /* No arena (allocation failed): fall back to a direct locked put. */
            free(a->spans); free(a->fill); a->spans = NULL; a->fill = NULL;
            struct ytelemetry_stored_span one;
            ytelemetry_fill_span(&one, root, trace, span);
            struct ytel_shard *sh = &s->shards[k];
            pthread_mutex_lock(&sh->mu);
            ytelemetry_shard_put(sh, s->shard_cap, &one);
            pthread_mutex_unlock(&sh->mu);
            return 1;
        }
    }

    /* Lock-free hot path: fill the parsed span into this worker's own bucket. */
    struct ytelemetry_stored_span *slot = &a->spans[(size_t)k * a->bucket_spans + a->fill[k]];
    ytelemetry_fill_span(slot, root, trace, span);
    a->fill[k]++;
    a->total++;

    if (a->fill[k] == a->bucket_spans) {
        ytelemetry_arena_flush_shard(s, a, k);
    } else if ((a->total & 63) == 0) {
        uint64_t now = ytelemetry_store_now_ns();
        if (now - a->last_flush_ns > s->flush_ns) ytelemetry_arena_flush_all(s, a, now);
    }
    return 1;
}

static void ytelemetry_count_malformed(struct ytelemetry_store *s)
{
    struct ytel_shard *sh = &s->shards[0];
    pthread_mutex_lock(&sh->mu); sh->malformed++; pthread_mutex_unlock(&sh->mu);
}

int ytelemetry_store_ingest_json(const char *json, size_t len)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    struct yjson_doc *doc = yjson_parse(json, len);
    if (!doc) { ytelemetry_count_malformed(s); return 0; }
    int r = ytelemetry_append_parsed(s, yjson_doc_root(doc));
    yjson_doc_free(doc);
    return r;
}

/* Ingest a batch: the payload may be a JSON array of span objects (the
 * batched sender path) or a single span object (back-compat). Parsed ONCE,
 * then each element flows through the lock-free arena append. */
int ytelemetry_store_ingest_batch_json(const char *json, size_t len)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    struct yjson_doc *doc = yjson_parse(json, len);
    if (!doc) { ytelemetry_count_malformed(s); return 0; }
    const struct yjson_value *root = yjson_doc_root(doc);
    int n = 0;
    if (yjson_is_array(root)) {
        size_t cnt = yjson_array_size(root);
        for (size_t i = 0; i < cnt; ++i) n += ytelemetry_append_parsed(s, yjson_array_at(root, i));
    } else {
        n = ytelemetry_append_parsed(s, root);
    }
    yjson_doc_free(doc);
    return n;
}

/* {ingested, malformed, evicted, stored, capacity, max_age_seconds} */
void ytelemetry_store_write_stats(struct yjson_writer *w)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    ytelemetry_store_ensure(s);
    uint64_t ingested = 0, malformed = 0, evicted = 0, stored = 0;
    for (unsigned k = 0; k < s->shard_count; ++k) {
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
    yjson_writer_key(w, "capacity"); yjson_writer_int(w, (int64_t)(s->shard_cap * s->shard_count));
    yjson_writer_key(w, "max_age_seconds"); yjson_writer_int(w, (int64_t)(s->max_age_ns / 1000000000ull));
    yjson_writer_key(w, "shards"); yjson_writer_int(w, (int64_t)s->shard_count);
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
    struct ytel_shard *sh = &s->shards[ytelemetry_shard_for(trace_id ? trace_id : "", s->shard_count)];
    pthread_mutex_lock(&sh->mu);

    /* The index tells us how many live spans to expect, so the scan stops as
     * soon as they are all found (recent traces sit near the ring front). */
    struct ytel_trace_idx *idx = NULL;
    if (trace_id) HASH_FIND_STR(sh->index, trace_id, idx);
    size_t want = idx ? idx->live : 0;

    enum { CAP = 8192 };
    struct ytelemetry_stored_span *match[CAP];
    size_t n = 0;
    for (size_t i = 0; i < sh->count && n < CAP && n < want; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i); /* newest first */
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

/* One summarised trace, copied out so the shard lock is released before merge. */
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

    enum { PER_SHARD = 64 };
    struct ytel_trace_sum *cand = malloc((size_t)s->shard_count * PER_SHARD * sizeof(*cand));
    size_t ncand = 0;

    if (cand) {
        for (unsigned k = 0; k < s->shard_count; ++k) {
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

                /* Summary is precomputed in the index — no per-trace ring scan. */
                struct ytel_trace_idx *e = NULL;
                HASH_FIND_STR(sh->index, sp->trace_id, e);
                if (!e) continue; /* span racing its own eviction — skip */

                struct ytel_trace_sum sum = {0};
                ytelemetry_store_copystr(sum.trace_id, sizeof(sum.trace_id), sp->trace_id);
                ytelemetry_store_copystr(sum.root_name, sizeof(sum.root_name), e->root_name);
                ytelemetry_store_copystr(sum.root_service, sizeof(sum.root_service), e->root_service);
                sum.start = e->start;
                sum.end = e->end;
                sum.span_count = e->live;
                sum.error = e->error;

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
    for (unsigned k = 0; k < s->shard_count; ++k) {
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
    for (unsigned k = 0; k < s->shard_count; ++k) {
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
    for (unsigned k = 0; k < s->shard_count; ++k) {
        pthread_mutex_lock(&s->shards[k].mu);
        total += s->shards[k].count;
        pthread_mutex_unlock(&s->shards[k].mu);
    }

    uint64_t *durs = malloc((total ? total : 1) * sizeof(uint64_t));
    size_t n = 0;
    if (durs) {
        for (unsigned k = 0; k < s->shard_count; ++k) {
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

    enum { PER_SHARD = 64 };
    struct ytelemetry_stored_span *errs = malloc((size_t)s->shard_count * PER_SHARD * sizeof(*errs));
    size_t nerr = 0;
    if (errs) {
        for (unsigned k = 0; k < s->shard_count; ++k) {
            struct ytel_shard *sh = &s->shards[k];
            pthread_mutex_lock(&sh->mu);
            size_t taken = 0;
            for (size_t i = 0; i < sh->count && taken < PER_SHARD; ++i) {
                struct ytelemetry_stored_span *sp = ytelemetry_shard_nth(sh, i); /* newest first */
                if (!sp->used || strcmp(sp->status, "error") != 0) continue;
                if (since_ns && sp->start_unix_ns < since_ns) continue;
                errs[nerr++] = *sp;
                taken++;
            }
            pthread_mutex_unlock(&sh->mu);
        }
    }

    yjson_writer_begin_object(w);
    yjson_writer_key(w, "errors");
    yjson_writer_begin_array(w);
    if (errs) {
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
