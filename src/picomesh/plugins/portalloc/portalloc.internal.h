/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `portalloc`.
 * NEVER include this from outside src/picomesh/plugins/portalloc/. */
#ifndef PICOMESH_PORTALLOC_INTERNAL_H
#define PICOMESH_PORTALLOC_INTERNAL_H

#include <picomesh/plugin/portalloc/portalloc.h>

typedef struct picomesh_uint32_result (*portalloc_portalloc_allocate_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*portalloc_portalloc_release_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*portalloc_portalloc_count_used_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_json_result (*portalloc_portalloc_list_fn)(struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);
typedef struct picomesh_json_result (*portalloc_portalloc_list_all_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
