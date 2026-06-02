/* ytelemetry_store — the trace collector's in-memory span store.
 *
 * A process-global, bounded ring of structured spans ingested from the
 * mesh (one JSON object per span, posted to the collector's /v1/spans).
 * Indexing is a linear scan over the ring — O(n) over a bounded set,
 * which is plenty for a dev/in-memory collector; there is deliberately
 * no durable storage and no secondary index. When the ring is full the
 * oldest span is evicted; on query, spans older than max_age are skipped.
 *
 * The query writers emit JSON straight into a yjson_writer the HTTP
 * layer owns, in shapes a waterfall/aggregate UI can render. */

#ifndef PICOMESH_YCORE_YTELEMETRY_STORE_H
#define PICOMESH_YCORE_YTELEMETRY_STORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct yjson_writer;

/* Size the ring and set retention. Safe to call once; calling again is a
 * no-op (the first sizing wins). If a query/ingest runs before init, the
 * store lazily inits with defaults. max_spans 0 ⇒ default; max_age 0 ⇒ no
 * age limit. */
void ytelemetry_store_init(size_t max_spans, uint64_t max_age_seconds);

/* Ingest one JSON span object (the body of one NDJSON line). Returns 1 if
 * stored, 0 if malformed/missing required fields. */
int ytelemetry_store_ingest_json(const char *json, size_t len);

/* ---- query writers (emit into a caller-owned yjson_writer) ----------- */

/* {trace_id, root_span_id, duration_ns, span_count, spans:[...]} */
void ytelemetry_store_write_trace(struct yjson_writer *w, const char *trace_id);
/* {traces:[{trace_id, root_name, service_name, start_time_ns, duration_ns,
 *           span_count, status}...]} filtered by service/since/status. Any
 * filter NULL/0 means "any". */
void ytelemetry_store_write_traces(struct yjson_writer *w, const char *service,
                             uint64_t since_ns, const char *status);
/* {services:[{service_name, span_count}...]} */
void ytelemetry_store_write_services(struct yjson_writer *w);
/* {service, operations:[{name, count}...]} */
void ytelemetry_store_write_operations(struct yjson_writer *w, const char *service);
/* {service, operation, window_ns, count, p50_ns, p90_ns, p99_ns, max_ns} */
void ytelemetry_store_write_latency(struct yjson_writer *w, const char *service,
                             const char *operation, uint64_t window_ns);
/* {errors:[{...span...}...]} with status=error and start >= since_ns. */
void ytelemetry_store_write_errors(struct yjson_writer *w, uint64_t since_ns);
/* {ingested, malformed, evicted, stored, capacity, max_age_seconds} — health
 * counters so dropped/malformed spans are not invisible. */
void ytelemetry_store_write_stats(struct yjson_writer *w);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_YCORE_YTELEMETRY_STORE_H */
