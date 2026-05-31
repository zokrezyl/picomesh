/* GENERATED — do not edit. */
#include "storage.internal.h"

__attribute__((unused))
static storage_set_fn _storage_db_storage_set_check = storage_set_impl;
__attribute__((unused))
static storage_get_fn _storage_db_storage_get_check = storage_get_impl;
__attribute__((unused))
static storage_exists_fn _storage_db_storage_exists_check = storage_exists_impl;
__attribute__((unused))
static storage_del_fn _storage_db_storage_del_check = storage_del_impl;
__attribute__((unused))
static storage_count_fn _storage_db_storage_count_check = storage_count_impl;

struct class_ptr_result storage_db_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return PICOMESH_OK(class_ptr, cls);
    ydebug("registering class=storage_db");

    static const struct class_descriptor desc = {
        .name = "storage_db",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct storage_data),
    };
    static const struct op ops[] = {
        {"storage", "set", (method_id_t)storage_set, (impl_t)storage_set_impl},
        {"storage", "get", (method_id_t)storage_get, (impl_t)storage_get_impl},
        {"storage", "exists", (method_id_t)storage_exists, (impl_t)storage_exists_impl},
        {"storage", "del", (method_id_t)storage_del, (impl_t)storage_del_impl},
        {"storage", "count", (method_id_t)storage_count, (impl_t)storage_count_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (PICOMESH_IS_ERR(_r))
        return PICOMESH_ERR(class_ptr, "storage_db_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
