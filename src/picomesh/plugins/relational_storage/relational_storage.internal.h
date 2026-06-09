/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `relational_storage`.
 * NEVER include this from outside src/picomesh/plugins/relational_storage/. */
#ifndef PICOMESH_RELATIONAL_STORAGE_INTERNAL_H
#define PICOMESH_RELATIONAL_STORAGE_INTERNAL_H

#include <picomesh/plugin/relational_storage/relational_storage.h>

typedef struct picomesh_json_result (*relational_storage_db_exec_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *, uint32_t,
    const char *, const char *);
typedef struct picomesh_json_result (*relational_storage_db_query_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *, uint32_t,
    const char *, const char *);
typedef struct picomesh_int_result (*relational_storage_db_shard_count_fn)(
    struct ctx *, struct object *, struct yheaders *, const char *);

#endif
