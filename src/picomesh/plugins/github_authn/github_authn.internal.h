/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `github_authn`.
 * NEVER include this from outside src/picomesh/plugins/github_authn/. */
#ifndef PICOMESH_GITHUB_AUTHN_INTERNAL_H
#define PICOMESH_GITHUB_AUTHN_INTERNAL_H

#include <picomesh/plugin/github_authn/github_authn.h>

typedef struct picomesh_json_result (
    *github_authn_github_authn_exchange_code_fn)(struct ctx *, struct object *,
                                                 struct yheaders *,
                                                 const char *, const char *);
typedef struct picomesh_int_result (
    *github_authn_github_authn_set_credentials_fn)(struct ctx *,
                                                   struct object *,
                                                   struct yheaders *, uint32_t,
                                                   uint32_t);
typedef struct picomesh_int_result (
    *github_authn_github_authn_register_code_fn)(struct ctx *, struct object *,
                                                 struct yheaders *, uint32_t,
                                                 uint32_t);
typedef struct picomesh_uint32_result (*github_authn_github_authn_resolve_fn)(
    struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*github_authn_github_authn_count_codes_fn)(
    struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_json_result (*github_authn_github_authn_list_fn)(
    struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);
typedef struct picomesh_json_result (*github_authn_github_authn_list_all_fn)(
    struct ctx *, struct object *, struct yheaders *);

#endif
