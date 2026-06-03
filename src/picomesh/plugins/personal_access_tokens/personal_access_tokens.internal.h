/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `personal_access_tokens`.
 * NEVER include this from outside src/picomesh/plugins/personal_access_tokens/. */
#ifndef PICOMESH_PERSONAL_ACCESS_TOKENS_INTERNAL_H
#define PICOMESH_PERSONAL_ACCESS_TOKENS_INTERNAL_H

#include <picomesh/plugin/personal_access_tokens/personal_access_tokens.h>

typedef struct picomesh_uint32_result (*personal_access_tokens_personal_access_tokens_mint_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_uint32_result (*personal_access_tokens_personal_access_tokens_lookup_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*personal_access_tokens_personal_access_tokens_revoke_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*personal_access_tokens_personal_access_tokens_list_for_user_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*personal_access_tokens_personal_access_tokens_count_active_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_json_result (*personal_access_tokens_personal_access_tokens_list_fn)(struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);
typedef struct picomesh_json_result (*personal_access_tokens_personal_access_tokens_list_all_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
