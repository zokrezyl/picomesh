/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `accounts`.
 * NEVER include this from outside src/picomesh/plugins/accounts/. */
#ifndef PICOMESH_ACCOUNTS_INTERNAL_H
#define PICOMESH_ACCOUNTS_INTERNAL_H

#include <picomesh/plugin/accounts/accounts.h>

typedef struct picomesh_int_result (*accounts_store_register_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*accounts_store_exists_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*accounts_store_set_balance_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, int64_t);
typedef struct picomesh_int64_result (*accounts_store_balance_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*accounts_store_count_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_string_result (*accounts_store_list_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
