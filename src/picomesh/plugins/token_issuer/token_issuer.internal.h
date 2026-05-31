/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `token_issuer`.
 * NEVER include this from outside src/picomesh/plugins/token_issuer/. */
#ifndef PICOMESH_TOKEN_ISSUER_INTERNAL_H
#define PICOMESH_TOKEN_ISSUER_INTERNAL_H

#include <picomesh/plugin/token_issuer/token_issuer.h>

typedef struct picomesh_uint32_result (*token_issuer_store_login_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, uint32_t);
typedef struct picomesh_uint32_result (*token_issuer_store_validate_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_uint32_result (*token_issuer_store_refresh_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*token_issuer_store_revoke_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*token_issuer_store_count_active_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
