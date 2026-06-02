/* ytelemetry_store — in-memory trace collector store (see ytelemetry_store.h). */

#include <picomesh/ycore/ytelemetry_store.h>

#include <picomesh/yjson/yjson.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define YTEL_STORE_DEFAULT_MAX 50000

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

struct ytelemetry_store {
    pthread_mutex_t mu;
    struct ytelemetry_stored_span *ring;
    size_t cap;
    size_t head;  /* next write slot */
    size_t count; /* live entries (<= cap) */
    uint64_t max_age_ns;
    int inited;
    /* health counters (read via ytelemetry_store_write_stats). */
    uint64_t ingested;  /* spans accepted */
    uint64_t malformed; /* rejected: bad JSON or missing required fields */
    uint64_t evicted;   /* dropped to make room when the ring was full */
};

static struct ytelemetry_store *ytelemetry_store_state(void)
{
    static struct ytelemetry_store s = {.mu = PTHREAD_MUTEX_INITIALIZER};
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

/* Caller must hold the lock. */
static void ytelemetry_store_ensure_locked(struct ytelemetry_store *s)
{
    if (s->inited) return;
    if (!s->cap) s->cap = YTEL_STORE_DEFAULT_MAX;
    s->ring = calloc(s->cap, sizeof(*s->ring));
    if (!s->ring) { s->cap = 0; return; }
    s->inited = 1;
}

void ytelemetry_store_init(size_t max_spans, uint64_t max_age_seconds)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);
    if (!s->inited) {
        s->cap = max_spans ? max_spans : YTEL_STORE_DEFAULT_MAX;
        s->max_age_ns = max_age_seconds * 1000000000ull;
        ytelemetry_store_ensure_locked(s);
    }
    pthread_mutex_unlock(&s->mu);
}

/* The i-th newest live span (0 = newest). Caller holds the lock. */
static struct ytelemetry_stored_span *ytelemetry_store_nth(struct ytelemetry_store *s, size_t i)
{
    if (i >= s->count) return NULL;
    size_t idx = (s->head + s->cap - 1 - i) % s->cap;
    return &s->ring[idx];
}

static int ytelemetry_store_fresh(const struct ytelemetry_store *s, const struct ytelemetry_stored_span *sp,
                            uint64_t now)
{
    if (!s->max_age_ns) return 1;
    return sp->start_unix_ns + s->max_age_ns >= now;
}

int ytelemetry_store_ingest_json(const char *json, size_t len)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    struct yjson_doc *doc = yjson_parse(json, len);
    if (!doc) {
        pthread_mutex_lock(&s->mu); s->malformed++; pthread_mutex_unlock(&s->mu);
        return 0;
    }
    const struct yjson_value *root = yjson_doc_root(doc);
    const char *trace = yjson_as_string(yjson_object_get(root, "trace_id"), NULL);
    const char *span = yjson_as_string(yjson_object_get(root, "span_id"), NULL);
    if (!trace || !span) {
        yjson_doc_free(doc);
        pthread_mutex_lock(&s->mu); s->malformed++; pthread_mutex_unlock(&s->mu);
        return 0;
    }

    pthread_mutex_lock(&s->mu);
    ytelemetry_store_ensure_locked(s);
    if (!s->ring) { s->malformed++; pthread_mutex_unlock(&s->mu); yjson_doc_free(doc); return 0; }
    if (s->count == s->cap) s->evicted++; /* overwriting the oldest live span */

    struct ytelemetry_stored_span *dst = &s->ring[s->head];
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

    s->head = (s->head + 1) % s->cap;
    if (s->count < s->cap) s->count++;
    s->ingested++;
    pthread_mutex_unlock(&s->mu);

    yjson_doc_free(doc);
    return 1;
}

/* {ingested, malformed, evicted, stored, capacity, max_age_seconds} */
void ytelemetry_store_write_stats(struct yjson_writer *w)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);
    yjson_w_begin_object(w);
    yjson_w_key(w, "ingested"); yjson_w_int(w, (int64_t)s->ingested);
    yjson_w_key(w, "malformed"); yjson_w_int(w, (int64_t)s->malformed);
    yjson_w_key(w, "evicted"); yjson_w_int(w, (int64_t)s->evicted);
    yjson_w_key(w, "stored"); yjson_w_int(w, (int64_t)s->count);
    yjson_w_key(w, "capacity"); yjson_w_int(w, (int64_t)s->cap);
    yjson_w_key(w, "max_age_seconds"); yjson_w_int(w, (int64_t)(s->max_age_ns / 1000000000ull));
    yjson_w_end_object(w);
    pthread_mutex_unlock(&s->mu);
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
    yjson_w_begin_object(w);
    yjson_w_key(w, "span_id"); yjson_w_string(w, sp->span_id);
    yjson_w_key(w, "parent_span_id"); yjson_w_string(w, sp->parent_id);
    yjson_w_key(w, "trace_id"); yjson_w_string(w, sp->trace_id);
    yjson_w_key(w, "name"); yjson_w_string(w, sp->name);
    yjson_w_key(w, "kind"); yjson_w_string(w, sp->kind);
    yjson_w_key(w, "service_name"); yjson_w_string(w, sp->service);
    yjson_w_key(w, "node_id"); yjson_w_string(w, sp->node);
    yjson_w_key(w, "start_time_ns"); yjson_w_int(w, (int64_t)sp->start_unix_ns);
    yjson_w_key(w, "duration_ns"); yjson_w_int(w, (int64_t)sp->duration_ns);
    yjson_w_key(w, "status"); yjson_w_string(w, sp->status);
    if (sp->err[0]) { yjson_w_key(w, "error_message"); yjson_w_string(w, sp->err); }
    yjson_w_key(w, "attributes");
    yjson_w_begin_object(w);
    yjson_w_key(w, "picomesh.uid"); yjson_w_int(w, (int64_t)sp->uid);
    yjson_w_end_object(w);
    yjson_w_end_object(w);
}

void ytelemetry_store_write_trace(struct yjson_writer *w, const char *trace_id)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);

    /* Collect the trace's spans (oldest→newest) so we can pick a root and
     * total duration before emitting. A trace is bounded; cap defensively. */
    enum { CAP = 4096 };
    struct ytelemetry_stored_span *match[CAP];
    size_t n = 0;
    for (size_t i = 0; i < s->count && n < CAP; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_store_nth(s, s->count - 1 - i);
        if (sp->used && trace_id && strcmp(sp->trace_id, trace_id) == 0)
            match[n++] = sp;
    }

    uint64_t min_start = 0, max_end = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t end = match[i]->start_unix_ns + match[i]->duration_ns;
        if (i == 0 || match[i]->start_unix_ns < min_start) min_start = match[i]->start_unix_ns;
        if (i == 0 || end > max_end) max_end = end;
    }

    /* Root = a parentless span, else one whose parent is outside this trace
     * (a continued/remote-parent trace), else nothing. */
    const char *root_span = "";
    for (size_t i = 0; i < n && !root_span[0]; ++i)
        if (!match[i]->parent_id[0]) root_span = match[i]->span_id;
    for (size_t i = 0; i < n && !root_span[0]; ++i) {
        int parent_in = 0;
        for (size_t j = 0; j < n; ++j)
            if (strcmp(match[j]->span_id, match[i]->parent_id) == 0) { parent_in = 1; break; }
        if (!parent_in) root_span = match[i]->span_id;
    }

    yjson_w_begin_object(w);
    yjson_w_key(w, "trace_id"); yjson_w_string(w, trace_id ? trace_id : "");
    yjson_w_key(w, "root_span_id"); yjson_w_string(w, root_span);
    yjson_w_key(w, "span_count"); yjson_w_int(w, (int64_t)n);
    yjson_w_key(w, "duration_ns"); yjson_w_int(w, (int64_t)(n ? max_end - min_start : 0));
    yjson_w_key(w, "spans");
    yjson_w_begin_array(w);
    for (size_t i = 0; i < n; ++i) emit_span(w, match[i]);
    yjson_w_end_array(w);
    yjson_w_end_object(w);

    pthread_mutex_unlock(&s->mu);
}

void ytelemetry_store_write_traces(struct yjson_writer *w, const char *service,
                             uint64_t since_ns, const char *status)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);
    uint64_t now = ytelemetry_store_now_ns();

    /* Bounded set of distinct trace ids, newest-first. */
    enum { MAX_TRACES = 256 };
    char seen[MAX_TRACES][33];
    size_t nseen = 0;

    yjson_w_begin_object(w);
    yjson_w_key(w, "traces");
    yjson_w_begin_array(w);

    for (size_t i = 0; i < s->count && nseen < MAX_TRACES; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_store_nth(s, i); /* newest→oldest */
        if (!sp->used || !ytelemetry_store_fresh(s, sp, now)) continue;
        if (since_ns && sp->start_unix_ns < since_ns) continue;
        if (service && service[0] && strcmp(sp->service, service) != 0) continue;

        int dup = 0;
        for (size_t j = 0; j < nseen; ++j)
            if (strcmp(seen[j], sp->trace_id) == 0) { dup = 1; break; }
        if (dup) continue;

        /* Summarise this whole trace. */
        const char *root_name = sp->name, *root_service = sp->service;
        uint64_t tstart = sp->start_unix_ns, tend = sp->start_unix_ns + sp->duration_ns;
        size_t span_count = 0;
        int trace_error = 0;
        for (size_t k = 0; k < s->count; ++k) {
            struct ytelemetry_stored_span *o = ytelemetry_store_nth(s, k);
            if (!o->used || strcmp(o->trace_id, sp->trace_id) != 0) continue;
            span_count++;
            if (o->start_unix_ns < tstart) tstart = o->start_unix_ns;
            uint64_t e = o->start_unix_ns + o->duration_ns;
            if (e > tend) tend = e;
            if (strcmp(o->status, "error") == 0) trace_error = 1;
            if (!o->parent_id[0]) { root_name = o->name; root_service = o->service; }
        }
        const char *tstatus = trace_error ? "error" : "ok";
        if (status && status[0] && strcmp(status, tstatus) != 0) continue;

        ytelemetry_store_copystr(seen[nseen], sizeof(seen[nseen]), sp->trace_id);
        nseen++;

        yjson_w_begin_object(w);
        yjson_w_key(w, "trace_id"); yjson_w_string(w, sp->trace_id);
        yjson_w_key(w, "root_name"); yjson_w_string(w, root_name);
        yjson_w_key(w, "service_name"); yjson_w_string(w, root_service);
        yjson_w_key(w, "start_time_ns"); yjson_w_int(w, (int64_t)tstart);
        yjson_w_key(w, "duration_ns"); yjson_w_int(w, (int64_t)(tend - tstart));
        yjson_w_key(w, "span_count"); yjson_w_int(w, (int64_t)span_count);
        yjson_w_key(w, "status"); yjson_w_string(w, tstatus);
        yjson_w_end_object(w);
    }

    yjson_w_end_array(w);
    yjson_w_end_object(w);
    pthread_mutex_unlock(&s->mu);
}

void ytelemetry_store_write_services(struct yjson_writer *w)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);

    enum { MAX_SVC = 128 };
    char names[MAX_SVC][64];
    size_t counts[MAX_SVC] = {0};
    size_t n = 0;
    for (size_t i = 0; i < s->count; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_store_nth(s, i);
        if (!sp->used || !sp->service[0]) continue;
        size_t j = 0;
        for (; j < n; ++j) if (strcmp(names[j], sp->service) == 0) break;
        if (j == n && n < MAX_SVC) { ytelemetry_store_copystr(names[n], 64, sp->service); n++; }
        if (j < MAX_SVC) counts[j]++;
    }

    yjson_w_begin_object(w);
    yjson_w_key(w, "services");
    yjson_w_begin_array(w);
    for (size_t j = 0; j < n; ++j) {
        yjson_w_begin_object(w);
        yjson_w_key(w, "service_name"); yjson_w_string(w, names[j]);
        yjson_w_key(w, "span_count"); yjson_w_int(w, (int64_t)counts[j]);
        yjson_w_end_object(w);
    }
    yjson_w_end_array(w);
    yjson_w_end_object(w);
    pthread_mutex_unlock(&s->mu);
}

void ytelemetry_store_write_operations(struct yjson_writer *w, const char *service)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);

    enum { MAX_OPS = 256 };
    char ops[MAX_OPS][64];
    size_t counts[MAX_OPS] = {0};
    size_t n = 0;
    for (size_t i = 0; i < s->count; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_store_nth(s, i);
        if (!sp->used || !sp->name[0]) continue;
        if (service && service[0] && strcmp(sp->service, service) != 0) continue;
        size_t j = 0;
        for (; j < n; ++j) if (strcmp(ops[j], sp->name) == 0) break;
        if (j == n && n < MAX_OPS) { ytelemetry_store_copystr(ops[n], 64, sp->name); n++; }
        if (j < MAX_OPS) counts[j]++;
    }

    yjson_w_begin_object(w);
    yjson_w_key(w, "service"); yjson_w_string(w, service ? service : "");
    yjson_w_key(w, "operations");
    yjson_w_begin_array(w);
    for (size_t j = 0; j < n; ++j) {
        yjson_w_begin_object(w);
        yjson_w_key(w, "name"); yjson_w_string(w, ops[j]);
        yjson_w_key(w, "count"); yjson_w_int(w, (int64_t)counts[j]);
        yjson_w_end_object(w);
    }
    yjson_w_end_array(w);
    yjson_w_end_object(w);
    pthread_mutex_unlock(&s->mu);
}

void ytelemetry_store_write_latency(struct yjson_writer *w, const char *service,
                             const char *operation, uint64_t window_ns)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);
    uint64_t now = ytelemetry_store_now_ns();
    uint64_t floor = window_ns && now > window_ns ? now - window_ns : 0;

    uint64_t *durs = malloc((s->count ? s->count : 1) * sizeof(uint64_t));
    size_t n = 0;
    if (durs) {
        for (size_t i = 0; i < s->count; ++i) {
            struct ytelemetry_stored_span *sp = ytelemetry_store_nth(s, i);
            if (!sp->used) continue;
            if (floor && sp->start_unix_ns < floor) continue;
            if (service && service[0] && strcmp(sp->service, service) != 0) continue;
            if (operation && operation[0] && strcmp(sp->name, operation) != 0) continue;
            durs[n++] = sp->duration_ns;
        }
        qsort(durs, n, sizeof(uint64_t), cmp_u64);
    }

    yjson_w_begin_object(w);
    yjson_w_key(w, "service"); yjson_w_string(w, service ? service : "");
    yjson_w_key(w, "operation"); yjson_w_string(w, operation ? operation : "");
    yjson_w_key(w, "window_ns"); yjson_w_int(w, (int64_t)window_ns);
    yjson_w_key(w, "count"); yjson_w_int(w, (int64_t)n);
    yjson_w_key(w, "p50_ns"); yjson_w_int(w, (int64_t)pctl_u64(durs, n, 50));
    yjson_w_key(w, "p90_ns"); yjson_w_int(w, (int64_t)pctl_u64(durs, n, 90));
    yjson_w_key(w, "p99_ns"); yjson_w_int(w, (int64_t)pctl_u64(durs, n, 99));
    yjson_w_key(w, "max_ns"); yjson_w_int(w, (int64_t)(n ? durs[n - 1] : 0));
    yjson_w_end_object(w);

    free(durs);
    pthread_mutex_unlock(&s->mu);
}

void ytelemetry_store_write_errors(struct yjson_writer *w, uint64_t since_ns)
{
    struct ytelemetry_store *s = ytelemetry_store_state();
    pthread_mutex_lock(&s->mu);

    yjson_w_begin_object(w);
    yjson_w_key(w, "errors");
    yjson_w_begin_array(w);
    size_t emitted = 0;
    for (size_t i = 0; i < s->count && emitted < 256; ++i) {
        struct ytelemetry_stored_span *sp = ytelemetry_store_nth(s, i); /* newest first */
        if (!sp->used || strcmp(sp->status, "error") != 0) continue;
        if (since_ns && sp->start_unix_ns < since_ns) continue;
        emit_span(w, sp);
        emitted++;
    }
    yjson_w_end_array(w);
    yjson_w_end_object(w);
    pthread_mutex_unlock(&s->mu);
}
