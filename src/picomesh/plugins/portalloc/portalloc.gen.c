/* GENERATED — do not edit. */
#include "portalloc.internal.h"

__attribute__((unused))
static portalloc_portalloc_allocate_fn _portalloc_portalloc_portalloc_portalloc_allocate_check = portalloc_portalloc_allocate_impl;
__attribute__((unused))
static portalloc_portalloc_release_fn _portalloc_portalloc_portalloc_portalloc_release_check = portalloc_portalloc_release_impl;
__attribute__((unused))
static portalloc_portalloc_count_used_fn _portalloc_portalloc_portalloc_portalloc_count_used_check = portalloc_portalloc_count_used_impl;
__attribute__((unused))
static portalloc_portalloc_list_fn _portalloc_portalloc_portalloc_portalloc_list_check = portalloc_portalloc_list_impl;
__attribute__((unused))
static portalloc_portalloc_list_all_fn _portalloc_portalloc_portalloc_portalloc_list_all_check = portalloc_portalloc_list_all_impl;

struct class_ptr_result portalloc_portalloc_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return PICOMESH_OK(class_ptr, cls);
    ydebug("registering class=portalloc_portalloc");

    static const struct class_descriptor desc = {
        .name = "portalloc_portalloc",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct portalloc_portalloc_data),
    };
    static const struct op ops[] = {
        {"portalloc", "portalloc_allocate", (method_id_t)portalloc_portalloc_allocate, (impl_t)portalloc_portalloc_allocate_impl},
        {"portalloc", "portalloc_release", (method_id_t)portalloc_portalloc_release, (impl_t)portalloc_portalloc_release_impl},
        {"portalloc", "portalloc_count_used", (method_id_t)portalloc_portalloc_count_used, (impl_t)portalloc_portalloc_count_used_impl},
        {"portalloc", "portalloc_list", (method_id_t)portalloc_portalloc_list, (impl_t)portalloc_portalloc_list_impl},
        {"portalloc", "portalloc_list_all", (method_id_t)portalloc_portalloc_list_all, (impl_t)portalloc_portalloc_list_all_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (PICOMESH_IS_ERR(_r))
        return PICOMESH_ERR(class_ptr, "portalloc_portalloc_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
