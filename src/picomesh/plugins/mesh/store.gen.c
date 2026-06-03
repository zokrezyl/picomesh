/* GENERATED — do not edit. */
#include "mesh.internal.h"

__attribute__((unused))
static mesh_mesh_register_service_fn _mesh_mesh_mesh_mesh_register_service_check = mesh_mesh_register_service_impl;
__attribute__((unused))
static mesh_mesh_resolve_fn _mesh_mesh_mesh_mesh_resolve_check = mesh_mesh_resolve_impl;
__attribute__((unused))
static mesh_mesh_forget_fn _mesh_mesh_mesh_mesh_forget_check = mesh_mesh_forget_impl;
__attribute__((unused))
static mesh_mesh_count_services_fn _mesh_mesh_mesh_mesh_count_services_check = mesh_mesh_count_services_impl;
__attribute__((unused))
static mesh_mesh_spawn_picomesh_fn _mesh_mesh_mesh_mesh_spawn_picomesh_check = mesh_mesh_spawn_picomesh_impl;
__attribute__((unused))
static mesh_mesh_kill_pid_fn _mesh_mesh_mesh_mesh_kill_pid_check = mesh_mesh_kill_pid_impl;
__attribute__((unused))
static mesh_mesh_count_children_fn _mesh_mesh_mesh_mesh_count_children_check = mesh_mesh_count_children_impl;
__attribute__((unused))
static mesh_mesh_reconcile_from_config_fn _mesh_mesh_mesh_mesh_reconcile_from_config_check = mesh_mesh_reconcile_from_config_impl;
__attribute__((unused))
static mesh_mesh_reconcile_fn _mesh_mesh_mesh_mesh_reconcile_check = mesh_mesh_reconcile_impl;

struct class_ptr_result mesh_mesh_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return PICOMESH_OK(class_ptr, cls);
    ydebug("registering class=mesh_mesh");

    static const struct class_descriptor desc = {
        .name = "mesh_mesh",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct mesh_mesh_data),
    };
    static const struct op ops[] = {
        {"mesh", "mesh_register_service", (method_id_t)mesh_mesh_register_service, (impl_t)mesh_mesh_register_service_impl},
        {"mesh", "mesh_resolve", (method_id_t)mesh_mesh_resolve, (impl_t)mesh_mesh_resolve_impl},
        {"mesh", "mesh_forget", (method_id_t)mesh_mesh_forget, (impl_t)mesh_mesh_forget_impl},
        {"mesh", "mesh_count_services", (method_id_t)mesh_mesh_count_services, (impl_t)mesh_mesh_count_services_impl},
        {"mesh", "mesh_spawn_picomesh", (method_id_t)mesh_mesh_spawn_picomesh, (impl_t)mesh_mesh_spawn_picomesh_impl},
        {"mesh", "mesh_kill_pid", (method_id_t)mesh_mesh_kill_pid, (impl_t)mesh_mesh_kill_pid_impl},
        {"mesh", "mesh_count_children", (method_id_t)mesh_mesh_count_children, (impl_t)mesh_mesh_count_children_impl},
        {"mesh", "mesh_reconcile_from_config", (method_id_t)mesh_mesh_reconcile_from_config, (impl_t)mesh_mesh_reconcile_from_config_impl},
        {"mesh", "mesh_reconcile", (method_id_t)mesh_mesh_reconcile, (impl_t)mesh_mesh_reconcile_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (PICOMESH_IS_ERR(_r))
        return PICOMESH_ERR(class_ptr, "mesh_mesh_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
