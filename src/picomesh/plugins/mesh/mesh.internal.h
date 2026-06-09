/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `mesh`.
 * NEVER include this from outside src/picomesh/plugins/mesh/. */
#ifndef PICOMESH_MESH_INTERNAL_H
#define PICOMESH_MESH_INTERNAL_H

#include <picomesh/plugin/mesh/mesh.h>

typedef struct picomesh_int_result (*mesh_mesh_register_service_fn)(
    struct ctx *, struct object *, struct yheaders *, uint32_t, uint32_t);
typedef struct picomesh_uint32_result (*mesh_mesh_resolve_fn)(struct ctx *,
                                                              struct object *,
                                                              struct yheaders *,
                                                              uint32_t);
typedef struct picomesh_int_result (*mesh_mesh_forget_fn)(struct ctx *,
                                                          struct object *,
                                                          struct yheaders *,
                                                          uint32_t);
typedef struct picomesh_size_result (*mesh_mesh_count_services_fn)(
    struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_int_result (*mesh_mesh_spawn_picomesh_fn)(
    struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*mesh_mesh_kill_pid_fn)(struct ctx *,
                                                            struct object *,
                                                            struct yheaders *,
                                                            int32_t);
typedef struct picomesh_size_result (*mesh_mesh_count_children_fn)(
    struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_int_result (*mesh_mesh_reconcile_from_config_fn)(
    struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_int_result (*mesh_mesh_reconcile_fn)(struct ctx *,
                                                             struct object *,
                                                             struct yheaders *);

#endif
