/* GENERATED — do not edit. */
#include "trace_collector.internal.h"

__attribute__((unused))
static trace_collector_store_ingest_fn _trace_collector_store_trace_collector_store_ingest_check = trace_collector_store_ingest_impl;
__attribute__((unused))
static trace_collector_store_get_trace_fn _trace_collector_store_trace_collector_store_get_trace_check = trace_collector_store_get_trace_impl;
__attribute__((unused))
static trace_collector_store_services_fn _trace_collector_store_trace_collector_store_services_check = trace_collector_store_services_impl;
__attribute__((unused))
static trace_collector_store_operations_fn _trace_collector_store_trace_collector_store_operations_check = trace_collector_store_operations_impl;
__attribute__((unused))
static trace_collector_store_latency_fn _trace_collector_store_trace_collector_store_latency_check = trace_collector_store_latency_impl;
__attribute__((unused))
static trace_collector_store_stats_fn _trace_collector_store_trace_collector_store_stats_check = trace_collector_store_stats_impl;
__attribute__((unused))
static trace_collector_store_errors_fn _trace_collector_store_trace_collector_store_errors_check = trace_collector_store_errors_impl;

struct class_ptr_result trace_collector_store_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return PICOMESH_OK(class_ptr, cls);
    ydebug("registering class=trace_collector_store");

    static const struct class_descriptor desc = {
        .name = "trace_collector_store",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct trace_collector_store_data),
    };
    static const struct op ops[] = {
        {"trace_collector", "store_ingest", (method_id_t)trace_collector_store_ingest, (impl_t)trace_collector_store_ingest_impl},
        {"trace_collector", "store_get_trace", (method_id_t)trace_collector_store_get_trace, (impl_t)trace_collector_store_get_trace_impl},
        {"trace_collector", "store_services", (method_id_t)trace_collector_store_services, (impl_t)trace_collector_store_services_impl},
        {"trace_collector", "store_operations", (method_id_t)trace_collector_store_operations, (impl_t)trace_collector_store_operations_impl},
        {"trace_collector", "store_latency", (method_id_t)trace_collector_store_latency, (impl_t)trace_collector_store_latency_impl},
        {"trace_collector", "store_stats", (method_id_t)trace_collector_store_stats, (impl_t)trace_collector_store_stats_impl},
        {"trace_collector", "store_errors", (method_id_t)trace_collector_store_errors, (impl_t)trace_collector_store_errors_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (PICOMESH_IS_ERR(_r))
        return PICOMESH_ERR(class_ptr, "trace_collector_store_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
