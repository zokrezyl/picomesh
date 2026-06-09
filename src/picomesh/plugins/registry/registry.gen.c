/* GENERATED — do not edit. */
#include "registry.internal.h"

__attribute__((unused)) static registry_registry_register_service_fn
    _registry_registry_registry_registry_register_service_check =
        registry_registry_register_service_impl;
__attribute__((unused)) static registry_registry_deregister_service_fn
    _registry_registry_registry_registry_deregister_service_check =
        registry_registry_deregister_service_impl;
__attribute__((unused)) static registry_registry_resolve_fn
    _registry_registry_registry_registry_resolve_check =
        registry_registry_resolve_impl;
__attribute__((unused)) static registry_registry_discover_service_fn
    _registry_registry_registry_registry_discover_service_check =
        registry_registry_discover_service_impl;
__attribute__((unused)) static registry_registry_list_services_fn
    _registry_registry_registry_registry_list_services_check =
        registry_registry_list_services_impl;
__attribute__((unused)) static registry_registry_count_fn
    _registry_registry_registry_registry_count_check =
        registry_registry_count_impl;

struct class_ptr_result registry_registry_class_get(void) {
  static const struct class *cls = NULL;
  if (cls)
    return PICOMESH_OK(class_ptr, cls);
  ydebug("registering class=registry_registry");

  static const struct class_descriptor desc = {
      .name = "registry_registry",
      .type = CLASS_TYPE_REGULAR,
      .data_size = sizeof(struct registry_registry_data),
  };
  static const struct op ops[] = {
      {"registry", "registry_register_service",
       (method_id_t)registry_registry_register_service,
       (impl_t)registry_registry_register_service_impl},
      {"registry", "registry_deregister_service",
       (method_id_t)registry_registry_deregister_service,
       (impl_t)registry_registry_deregister_service_impl},
      {"registry", "registry_resolve", (method_id_t)registry_registry_resolve,
       (impl_t)registry_registry_resolve_impl},
      {"registry", "registry_discover_service",
       (method_id_t)registry_registry_discover_service,
       (impl_t)registry_registry_discover_service_impl},
      {"registry", "registry_list_services",
       (method_id_t)registry_registry_list_services,
       (impl_t)registry_registry_list_services_impl},
      {"registry", "registry_count", (method_id_t)registry_registry_count,
       (impl_t)registry_registry_count_impl},
  };
  struct class_ptr_result _r =
      class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]), NULL, NULL, 0);
  if (PICOMESH_IS_ERR(_r))
    return PICOMESH_ERR(
        class_ptr, "registry_registry_class_get: class_register failed", _r);
  cls = _r.value;
  return _r;
}
