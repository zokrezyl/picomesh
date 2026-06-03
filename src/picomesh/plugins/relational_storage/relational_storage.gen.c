/* GENERATED — do not edit. */
#include "relational_storage.internal.h"

__attribute__((unused))
static relational_storage_db_exec_fn _relational_storage_db_relational_storage_db_exec_check = relational_storage_db_exec_impl;
__attribute__((unused))
static relational_storage_db_query_fn _relational_storage_db_relational_storage_db_query_check = relational_storage_db_query_impl;

struct class_ptr_result relational_storage_db_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return PICOMESH_OK(class_ptr, cls);
    ydebug("registering class=relational_storage_db");

    static const struct class_descriptor desc = {
        .name = "relational_storage_db",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct relational_storage_data),
    };
    static const struct op ops[] = {
        {"relational_storage", "db_exec", (method_id_t)relational_storage_db_exec, (impl_t)relational_storage_db_exec_impl},
        {"relational_storage", "db_query", (method_id_t)relational_storage_db_query, (impl_t)relational_storage_db_query_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (PICOMESH_IS_ERR(_r))
        return PICOMESH_ERR(class_ptr, "relational_storage_db_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
