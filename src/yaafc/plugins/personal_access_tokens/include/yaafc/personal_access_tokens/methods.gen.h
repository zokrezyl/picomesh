/* GENERATED — do not edit. */
#ifndef YAAFC_PERSONAL_ACCESS_TOKENS_METHODS_GEN_H
#define YAAFC_PERSONAL_ACCESS_TOKENS_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result personal_access_tokens_store_mint(struct ctx * ctx, struct object * obj, uint32_t user_id);
typedef struct yaafc_uint32_result (*personal_access_tokens_store_mint_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_uint32_result personal_access_tokens_store_lookup(struct ctx * ctx, struct object * obj, uint32_t pat_id);
typedef struct yaafc_uint32_result (*personal_access_tokens_store_lookup_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result personal_access_tokens_store_revoke(struct ctx * ctx, struct object * obj, uint32_t pat_id);
typedef struct yaafc_int_result (*personal_access_tokens_store_revoke_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result personal_access_tokens_store_list_for_user(struct ctx * ctx, struct object * obj, uint32_t user_id);
typedef struct yaafc_size_result (*personal_access_tokens_store_list_for_user_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result personal_access_tokens_store_count_active(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*personal_access_tokens_store_count_active_fn)(struct ctx *, struct object *);

#endif
