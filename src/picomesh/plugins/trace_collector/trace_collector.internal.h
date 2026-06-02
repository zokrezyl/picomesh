/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `trace_collector`.
 * NEVER include this from outside src/picomesh/plugins/trace_collector/. */
#ifndef PICOMESH_TRACE_COLLECTOR_INTERNAL_H
#define PICOMESH_TRACE_COLLECTOR_INTERNAL_H

#include <picomesh/plugin/trace_collector/trace_collector.h>

typedef struct picomesh_void_result (*trace_collector_store_ingest_fn)(struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_string_result (*trace_collector_store_get_trace_fn)(struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_string_result (*trace_collector_store_services_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_string_result (*trace_collector_store_operations_fn)(struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_string_result (*trace_collector_store_latency_fn)(struct ctx *, struct object *, struct yheaders *, const char *, const char *, uint32_t);
typedef struct picomesh_string_result (*trace_collector_store_stats_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_string_result (*trace_collector_store_errors_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);

#endif
