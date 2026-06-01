/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `session`.
 * NEVER include this from outside src/picomesh/plugins/session/. */
#ifndef PICOMESH_SESSION_INTERNAL_H
#define PICOMESH_SESSION_INTERNAL_H

#include <picomesh/plugin/session/session.h>

typedef struct picomesh_string_result (*session_store_start_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, uint32_t);
typedef struct picomesh_uint32_result (*session_store_lookup_fn)(struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_int_result (*session_store_destroy_fn)(struct ctx *, struct object *, struct yheaders *, const char *);
typedef struct picomesh_size_result (*session_store_count_active_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
