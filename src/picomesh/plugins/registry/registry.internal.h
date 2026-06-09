/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `registry`.
 * NEVER include this from outside src/picomesh/plugins/registry/. */
#ifndef PICOMESH_REGISTRY_INTERNAL_H
#define PICOMESH_REGISTRY_INTERNAL_H

#include <picomesh/plugin/registry/registry.h>

typedef struct picomesh_int_result (*registry_registry_register_service_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *,
    const char *, const char *, uint32_t);
typedef struct picomesh_int_result (*registry_registry_deregister_service_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *,
    const char *);
typedef struct picomesh_string_result (*registry_registry_resolve_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_json_result (*registry_registry_discover_service_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_json_result (*registry_registry_list_services_fn)(
    struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_size_result (*registry_registry_count_fn)(
    struct ctx *, struct object *, struct yheaders *);

#endif
