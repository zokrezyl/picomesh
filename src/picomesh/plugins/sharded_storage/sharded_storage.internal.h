/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `sharded_storage`.
 * NEVER include this from outside src/picomesh/plugins/sharded_storage/. */
#ifndef PICOMESH_SHARDED_STORAGE_INTERNAL_H
#define PICOMESH_SHARDED_STORAGE_INTERNAL_H

#include <picomesh/plugin/sharded_storage/sharded_storage.h>

typedef struct picomesh_int_result (*sharded_storage_db_set_fn)(struct ctx *, struct object *, struct yheaders *, const char *, const char *, const char *);
typedef struct picomesh_string_result (*sharded_storage_db_get_fn)(struct ctx *, struct object *, struct yheaders *, const char *, const char *);
typedef struct picomesh_int_result (*sharded_storage_db_exists_fn)(struct ctx *, struct object *, struct yheaders *, const char *, const char *);
typedef struct picomesh_int_result (*sharded_storage_db_del_fn)(struct ctx *, struct object *, struct yheaders *, const char *, const char *);
typedef struct picomesh_size_result (*sharded_storage_db_count_fn)(struct ctx *, struct object *, struct yheaders *, const char *);

#endif
