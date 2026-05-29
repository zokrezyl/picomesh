/* GENERATED — do not edit. */
#include "sharded_storage.internal.h"

__attribute__((unused))
static sharded_storage_db_set_fn _sharded_storage_db_sharded_storage_db_set_check = sharded_storage_db_set_impl;
__attribute__((unused))
static sharded_storage_db_get_fn _sharded_storage_db_sharded_storage_db_get_check = sharded_storage_db_get_impl;
__attribute__((unused))
static sharded_storage_db_exists_fn _sharded_storage_db_sharded_storage_db_exists_check = sharded_storage_db_exists_impl;
__attribute__((unused))
static sharded_storage_db_del_fn _sharded_storage_db_sharded_storage_db_del_check = sharded_storage_db_del_impl;
__attribute__((unused))
static sharded_storage_db_count_fn _sharded_storage_db_sharded_storage_db_count_check = sharded_storage_db_count_impl;

struct class_ptr_result sharded_storage_db_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return YAAFC_OK(class_ptr, cls);
    ydebug("registering class=sharded_storage_db");

    static const struct class_descriptor desc = {
        .name = "sharded_storage_db",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct sharded_storage_data),
    };
    static const struct op ops[] = {
        {"sharded_storage", "db_set", (method_id_t)sharded_storage_db_set, (impl_t)sharded_storage_db_set_impl},
        {"sharded_storage", "db_get", (method_id_t)sharded_storage_db_get, (impl_t)sharded_storage_db_get_impl},
        {"sharded_storage", "db_exists", (method_id_t)sharded_storage_db_exists, (impl_t)sharded_storage_db_exists_impl},
        {"sharded_storage", "db_del", (method_id_t)sharded_storage_db_del, (impl_t)sharded_storage_db_del_impl},
        {"sharded_storage", "db_count", (method_id_t)sharded_storage_db_count, (impl_t)sharded_storage_db_count_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (YAAFC_IS_ERR(_r))
        return YAAFC_ERR(class_ptr, "sharded_storage_db_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
