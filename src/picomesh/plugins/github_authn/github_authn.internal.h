/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `github_authn`.
 * NEVER include this from outside src/picomesh/plugins/github_authn/. */
#ifndef PICOMESH_GITHUB_AUTHN_INTERNAL_H
#define PICOMESH_GITHUB_AUTHN_INTERNAL_H

#include <picomesh/plugin/github_authn/github_authn.h>

typedef struct picomesh_int_result (*github_authn_store_set_credentials_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, uint32_t);
typedef struct picomesh_int_result (*github_authn_store_register_code_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, uint32_t);
typedef struct picomesh_uint32_result (*github_authn_store_resolve_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*github_authn_store_count_codes_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
