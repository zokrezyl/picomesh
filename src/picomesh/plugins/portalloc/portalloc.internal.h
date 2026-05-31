/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `portalloc`.
 * NEVER include this from outside src/picomesh/plugins/portalloc/. */
#ifndef PICOMESH_PORTALLOC_INTERNAL_H
#define PICOMESH_PORTALLOC_INTERNAL_H

#include <picomesh/plugin/portalloc/portalloc.h>

typedef struct picomesh_uint32_result (*portalloc_store_allocate_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*portalloc_store_release_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*portalloc_store_count_used_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
