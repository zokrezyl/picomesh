/* GENERATED — do not edit. */
#ifndef YAAFC_PORTALLOC_METHODS_GEN_H
#define YAAFC_PORTALLOC_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result portalloc_store_allocate(struct ctx * ctx, struct object * obj, uint32_t service_id);
typedef struct yaafc_uint32_result (*portalloc_store_allocate_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result portalloc_store_release(struct ctx * ctx, struct object * obj, uint32_t port);
typedef struct yaafc_int_result (*portalloc_store_release_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result portalloc_store_count_used(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*portalloc_store_count_used_fn)(struct ctx *, struct object *);

#endif
