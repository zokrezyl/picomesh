/* GENERATED — do not edit. */
#include "trace_collector.internal.h"

__attribute__((unused)) static trace_collector_trace_collector_ingest_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_ingest_check =
        trace_collector_trace_collector_ingest_impl;
__attribute__((unused)) static trace_collector_trace_collector_get_trace_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_get_trace_check =
        trace_collector_trace_collector_get_trace_impl;
__attribute__((unused)) static trace_collector_trace_collector_traces_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_traces_check =
        trace_collector_trace_collector_traces_impl;
__attribute__((unused)) static trace_collector_trace_collector_services_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_services_check =
        trace_collector_trace_collector_services_impl;
__attribute__((unused)) static trace_collector_trace_collector_operations_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_operations_check =
        trace_collector_trace_collector_operations_impl;
__attribute__((unused)) static trace_collector_trace_collector_latency_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_latency_check =
        trace_collector_trace_collector_latency_impl;
__attribute__((unused)) static trace_collector_trace_collector_stats_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_stats_check =
        trace_collector_trace_collector_stats_impl;
__attribute__((unused)) static trace_collector_trace_collector_errors_fn
    _trace_collector_trace_collector_trace_collector_trace_collector_errors_check =
        trace_collector_trace_collector_errors_impl;

struct class_ptr_result trace_collector_trace_collector_class_get(void) {
  static const struct class *cls = NULL;
  if (cls)
    return PICOMESH_OK(class_ptr, cls);
  ydebug("registering class=trace_collector_trace_collector");

  static const struct class_descriptor desc = {
      .name = "trace_collector_trace_collector",
      .type = CLASS_TYPE_REGULAR,
      .data_size = sizeof(struct trace_collector_trace_collector_data),
  };
  static const struct op ops[] = {
      {"trace_collector", "trace_collector_ingest",
       (method_id_t)trace_collector_trace_collector_ingest,
       (impl_t)trace_collector_trace_collector_ingest_impl},
      {"trace_collector", "trace_collector_get_trace",
       (method_id_t)trace_collector_trace_collector_get_trace,
       (impl_t)trace_collector_trace_collector_get_trace_impl},
      {"trace_collector", "trace_collector_traces",
       (method_id_t)trace_collector_trace_collector_traces,
       (impl_t)trace_collector_trace_collector_traces_impl},
      {"trace_collector", "trace_collector_services",
       (method_id_t)trace_collector_trace_collector_services,
       (impl_t)trace_collector_trace_collector_services_impl},
      {"trace_collector", "trace_collector_operations",
       (method_id_t)trace_collector_trace_collector_operations,
       (impl_t)trace_collector_trace_collector_operations_impl},
      {"trace_collector", "trace_collector_latency",
       (method_id_t)trace_collector_trace_collector_latency,
       (impl_t)trace_collector_trace_collector_latency_impl},
      {"trace_collector", "trace_collector_stats",
       (method_id_t)trace_collector_trace_collector_stats,
       (impl_t)trace_collector_trace_collector_stats_impl},
      {"trace_collector", "trace_collector_errors",
       (method_id_t)trace_collector_trace_collector_errors,
       (impl_t)trace_collector_trace_collector_errors_impl},
  };
  struct class_ptr_result _r =
      class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]), NULL, NULL, 0);
  if (PICOMESH_IS_ERR(_r))
    return PICOMESH_ERR(
        class_ptr,
        "trace_collector_trace_collector_class_get: class_register failed", _r);
  cls = _r.value;
  return _r;
}
