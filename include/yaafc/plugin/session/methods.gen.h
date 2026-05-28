/* GENERATED — do not edit. */
#ifndef YAAFC_SESSION_METHODS_GEN_H
#define YAAFC_SESSION_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result session_store_start(struct ctx * ctx, struct object * obj, uint32_t user_id, uint32_t provider_id);
typedef struct yaafc_uint32_result (*session_store_start_fn)(struct ctx *, struct object *, uint32_t, uint32_t);
struct yaafc_uint32_result session_store_lookup(struct ctx * ctx, struct object * obj, uint32_t sid);
typedef struct yaafc_uint32_result (*session_store_lookup_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result session_store_destroy(struct ctx * ctx, struct object * obj, uint32_t sid);
typedef struct yaafc_int_result (*session_store_destroy_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result session_store_count_active(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*session_store_count_active_fn)(struct ctx *, struct object *);

#endif
