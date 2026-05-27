/* GENERATED — do not edit. */
#ifndef YAAFC_GITHUB_AUTHN_METHODS_GEN_H
#define YAAFC_GITHUB_AUTHN_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_int_result github_authn_store_set_credentials(struct ctx * ctx, struct object * obj, uint32_t client_id, uint32_t secret_id);
typedef struct yaafc_int_result (*github_authn_store_set_credentials_fn)(struct ctx *, struct object *, uint32_t, uint32_t);
struct yaafc_int_result github_authn_store_register_code(struct ctx * ctx, struct object * obj, uint32_t code, uint32_t user_id);
typedef struct yaafc_int_result (*github_authn_store_register_code_fn)(struct ctx *, struct object *, uint32_t, uint32_t);
struct yaafc_uint32_result github_authn_store_resolve(struct ctx * ctx, struct object * obj, uint32_t code);
typedef struct yaafc_uint32_result (*github_authn_store_resolve_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result github_authn_store_count_codes(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*github_authn_store_count_codes_fn)(struct ctx *, struct object *);

#endif
