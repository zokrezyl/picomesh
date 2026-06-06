/* trace_collector — the receiver side of Picomesh tracing (issue #11).
 *
 * A normal yrpc backend plugin: services emit finished spans by calling
 * `trace_collector_ingest` over yrpc (fire-and-forget, from ytelemetry);
 * operators
 * and the webapp read traces back through the query methods via the
 * gateway's /_rpc + /_describe — service-driven, no hand-rolled routes.
 *
 * State is an in-memory, bounded span store (ycore/ytelemetry_store). No
 * durable storage in v1. This process holds no other plugins and reaches
 * no backends. */

#include <stdio.h>
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/ytelemetry_store.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/yjson/yjson.h>
#include <picomesh/yplatform/time.h>

#include <stdlib.h>
#include <string.h>

struct PICOMESH_CLASS_ANNOTATE("class@trace_collector:trace_collector") trace_collector_trace_collector_data {
    char placeholder;
};

static uint64_t collector_now_ns(void)
{
    return (uint64_t)picomesh_yplatform_time_wall_ms() * 1000000ull;
}

/* Size the in-memory store from this process's config the first time the
 * collector is touched: telemetry.max_spans (ring size) + max_age_seconds
 * (retention). Idempotent (ytelemetry_store_init only honours the first
 * call); the `done` flag just skips the per-call config lookup. Safe across
 * the collector's worker threads — same value, idempotent init. */
static int64_t collector_cfg_int(const struct yconfig *cfg, const char *key)
{
    struct yconfig_node_ptr_result r = yconfig_get(cfg, key);
    return (PICOMESH_IS_OK(r) && r.value) ? yconfig_node_as_int(r.value, 0) : 0;
}

static void collector_ensure_store(void)
{
    static int done = 0;
    if (done) return;
    struct ytelemetry_store_config sc = {0};
    struct picomesh_engine *e = picomesh_active_engine();
    if (e) {
        const struct yconfig *cfg = picomesh_engine_config(e);
        int64_t v;
        if ((v = collector_cfg_int(cfg, "telemetry.max_spans")) > 0) sc.max_spans = (size_t)v;
        if ((v = collector_cfg_int(cfg, "telemetry.max_age_seconds")) > 0) sc.max_age_seconds = (uint64_t)v;
        if ((v = collector_cfg_int(cfg, "telemetry.shards")) > 0) sc.shards = (unsigned)v;
        if ((v = collector_cfg_int(cfg, "telemetry.bucket_spans")) > 0) sc.bucket_spans = (size_t)v;
        if ((v = collector_cfg_int(cfg, "telemetry.flush_ms")) > 0) sc.flush_ms = (uint64_t)v;
    }
    ytelemetry_store_init_config(&sc); /* any field left 0 takes its built-in default */
    done = 1;
}

/* Render a query-writer's JSON into an owned heap string the caller frees. */
static struct picomesh_string_result render(void (*emit)(struct yjson_writer *, void *),
                                            void *ud)
{
    collector_ensure_store(); /* config applies on first query too, not only ingest */
    struct yjson_writer *w = yjson_writer_new();
    if (!w) return PICOMESH_ERR(picomesh_string, "trace_collector: writer alloc failed");
    emit(w, ud);
    size_t len = 0;
    const char *data = yjson_writer_data(w, &len);
    char *out = malloc(len + 1);
    if (out) { memcpy(out, data, len); out[len] = 0; }
    yjson_writer_free(w);
    if (!out) return PICOMESH_ERR(picomesh_string, "trace_collector: out of memory");
    return PICOMESH_OK(picomesh_string, out);
}

/* ---- ingest ---------------------------------------------------------- */

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_ingest")
struct picomesh_void_result trace_collector_trace_collector_ingest_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs, const char *span_json)
{
    (void)ctx; (void)obj; (void)hdrs;
    collector_ensure_store();
    /* The store tallies malformed/evicted spans (queryable via store_stats),
     * so bad input is not invisible. Ingest is fire-and-forget at the wire,
     * so we still return OK regardless — the caller never waits on this.
     * The payload is a JSON array (the batched sender) or one span object
     * (back-compat); the batch entry point handles both and parses once. */
    if (span_json && *span_json)
        ytelemetry_store_ingest_batch_json(span_json, strlen(span_json));
    return PICOMESH_OK_VOID();
}

/* ---- queries --------------------------------------------------------- */

struct trace_arg { const char *a; const char *b; uint64_t n; };

static void emit_trace(struct yjson_writer *w, void *ud)
{
    ytelemetry_store_write_trace(w, ((struct trace_arg *)ud)->a);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_get_trace")
struct picomesh_string_result trace_collector_trace_collector_get_trace_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs, const char *trace_id)
{
    (void)ctx; (void)obj; (void)hdrs;
    struct trace_arg t = {.a = trace_id};
    return render(emit_trace, &t);
}

static void emit_traces(struct yjson_writer *w, void *ud)
{
    struct trace_arg *t = ud;
    ytelemetry_store_write_traces(w, (t->a && *t->a) ? t->a : NULL,
                                  t->n,
                                  (t->b && *t->b) ? t->b : NULL);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_traces")
struct picomesh_string_result trace_collector_trace_collector_traces_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
    const char *service, const char *status, uint32_t since_secs)
{
    (void)ctx; (void)obj; (void)hdrs;
    uint64_t floor = 0;
    if (since_secs) {
        uint64_t window = (uint64_t)since_secs * 1000000000ull;
        uint64_t now = collector_now_ns();
        floor = now > window ? now - window : 0;
    }
    struct trace_arg t = {.a = service, .b = status, .n = floor};
    return render(emit_traces, &t);
}

static void emit_services(struct yjson_writer *w, void *ud)
{
    (void)ud;
    ytelemetry_store_write_services(w);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_services")
struct picomesh_string_result trace_collector_trace_collector_services_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj; (void)hdrs;
    return render(emit_services, NULL);
}

static void emit_operations(struct yjson_writer *w, void *ud)
{
    const char *svc = ((struct trace_arg *)ud)->a;
    ytelemetry_store_write_operations(w, (svc && *svc) ? svc : NULL);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_operations")
struct picomesh_string_result trace_collector_trace_collector_operations_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs, const char *service)
{
    (void)ctx; (void)obj; (void)hdrs;
    struct trace_arg t = {.a = service};
    return render(emit_operations, &t);
}

static void emit_latency(struct yjson_writer *w, void *ud)
{
    struct trace_arg *t = ud;
    ytelemetry_store_write_latency(w, (t->a && *t->a) ? t->a : NULL,
                                   (t->b && *t->b) ? t->b : NULL, t->n);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_latency")
struct picomesh_string_result trace_collector_trace_collector_latency_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
    const char *service, const char *operation, uint32_t window_secs)
{
    (void)ctx; (void)obj; (void)hdrs;
    struct trace_arg t = {.a = service, .b = operation,
                          .n = (uint64_t)window_secs * 1000000000ull};
    return render(emit_latency, &t);
}

static void emit_stats(struct yjson_writer *w, void *ud)
{
    (void)ud;
    ytelemetry_store_write_stats(w);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_stats")
struct picomesh_string_result trace_collector_trace_collector_stats_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj; (void)hdrs;
    collector_ensure_store();
    return render(emit_stats, NULL);
}

static void emit_errors(struct yjson_writer *w, void *ud)
{
    ytelemetry_store_write_errors(w, ((struct trace_arg *)ud)->n);
}

PICOMESH_CLASS_ANNOTATE("override@trace_collector:trace_collector:trace_collector_errors")
struct picomesh_string_result trace_collector_trace_collector_errors_impl(
    struct ctx *ctx, struct object *obj, struct yheaders *hdrs, uint32_t since_secs)
{
    (void)ctx; (void)obj; (void)hdrs;
    uint64_t floor = 0;
    if (since_secs) {
        uint64_t window = (uint64_t)since_secs * 1000000000ull;
        uint64_t now = collector_now_ns();
        floor = now > window ? now - window : 0;
    }
    struct trace_arg t = {.n = floor};
    return render(emit_errors, &t);
}

#include "collector.gen.c"
