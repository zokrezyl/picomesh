/* GENERATED — do not edit. */
#ifndef YAAFC_PASSWORD_AUTHN_METHODS_GEN_H
#define YAAFC_PASSWORD_AUTHN_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;

struct yaafc_int_result password_authn_store_register(struct ctx * ctx, struct object * obj, uint32_t user_id, int64_t hash);
typedef struct yaafc_int_result (*password_authn_store_register_fn)(struct ctx *, struct object *, uint32_t, int64_t);
struct yaafc_int_result password_authn_store_authenticate(struct ctx * ctx, struct object * obj, uint32_t user_id, int64_t hash);
typedef struct yaafc_int_result (*password_authn_store_authenticate_fn)(struct ctx *, struct object *, uint32_t, int64_t);
struct yaafc_int_result password_authn_store_change_password(struct ctx * ctx, struct object * obj, uint32_t user_id, int64_t hash);
typedef struct yaafc_int_result (*password_authn_store_change_password_fn)(struct ctx *, struct object *, uint32_t, int64_t);
struct yaafc_size_result password_authn_store_count_registered(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*password_authn_store_count_registered_fn)(struct ctx *, struct object *);

#endif
