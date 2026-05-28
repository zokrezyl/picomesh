/* GENERATED — do not edit. */
#ifndef YAAFC_TOKEN_ISSUER_METHODS_GEN_H
#define YAAFC_TOKEN_ISSUER_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result token_issuer_store_login(struct ctx * ctx, struct object * obj, uint32_t user_id, uint32_t provider_id);
typedef struct yaafc_uint32_result (*token_issuer_store_login_fn)(struct ctx *, struct object *, uint32_t, uint32_t);
struct yaafc_uint32_result token_issuer_store_validate(struct ctx * ctx, struct object * obj, uint32_t token_id);
typedef struct yaafc_uint32_result (*token_issuer_store_validate_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_uint32_result token_issuer_store_refresh(struct ctx * ctx, struct object * obj, uint32_t token_id);
typedef struct yaafc_uint32_result (*token_issuer_store_refresh_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result token_issuer_store_revoke(struct ctx * ctx, struct object * obj, uint32_t token_id);
typedef struct yaafc_int_result (*token_issuer_store_revoke_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result token_issuer_store_count_active(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*token_issuer_store_count_active_fn)(struct ctx *, struct object *);

#endif
