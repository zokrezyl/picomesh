/* GENERATED — do not edit. */
#ifndef YAAFC_ACCOUNTS_METHODS_GEN_H
#define YAAFC_ACCOUNTS_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int64_result;
struct yaafc_int_result;
struct yaafc_size_result;

struct yaafc_int_result accounts_store_register(struct ctx * ctx, struct object * obj, uint32_t uid);
typedef struct yaafc_int_result (*accounts_store_register_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result accounts_store_exists(struct ctx * ctx, struct object * obj, uint32_t uid);
typedef struct yaafc_int_result (*accounts_store_exists_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result accounts_store_set_balance(struct ctx * ctx, struct object * obj, uint32_t uid, int64_t n);
typedef struct yaafc_int_result (*accounts_store_set_balance_fn)(struct ctx *, struct object *, uint32_t, int64_t);
struct yaafc_int64_result accounts_store_balance(struct ctx * ctx, struct object * obj, uint32_t uid);
typedef struct yaafc_int64_result (*accounts_store_balance_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result accounts_store_count(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*accounts_store_count_fn)(struct ctx *, struct object *);

#endif
