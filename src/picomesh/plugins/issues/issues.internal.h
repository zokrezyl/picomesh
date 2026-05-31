/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `issues`.
 * NEVER include this from outside src/picomesh/plugins/issues/. */
#ifndef PICOMESH_ISSUES_INTERNAL_H
#define PICOMESH_ISSUES_INTERNAL_H

#include <picomesh/plugin/issues/issues.h>

typedef struct picomesh_uint32_result (*issues_store_open_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, uint32_t);
typedef struct picomesh_int_result (*issues_store_close_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*issues_store_status_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*issues_store_count_open_in_repo_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*issues_store_count_total_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
