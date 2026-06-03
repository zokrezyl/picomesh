/* Authorizer framework — base types.
 *
 * An authorizer takes the verified JWT (or NULL for anonymous), the endpoint
 * name, and the call args, and decides allow / deny. It runs after the
 * authenticator chain. Like authenticators, it is a pluggable framework
 * category selected by `type:` in config — there is exactly ONE authorizer per
 * frontend (unlike the authenticator chain). The decision is a pure function
 * of the loaded policy, the JWT claims, and the call shape. */

#ifndef PICOMESH_AUTHORIZERS_BASE_H
#define PICOMESH_AUTHORIZERS_BASE_H

#include <picomesh/ycore/result.h>

#ifdef __cplusplus
extern "C" {
#endif

struct picomesh_engine;
struct yconfig_node;
struct yjson_value;

/* Result of an authorize decision. `status` is the HTTP code a frontend maps a
 * denial to: 401 when authentication is required but absent/invalid, 403 when
 * the endpoint is absent from policy (default deny) or the role is
 * insufficient. `reason` is surfaced to the client — informative, no secrets. */
struct picomesh_authz_decision {
    int allowed;       /* 1 allow, 0 deny */
    int status;        /* 0 on allow; 401 or 403 on deny */
    char reason[160];
};

/* The ops a concrete authorizer type implements. */
struct picomesh_authorizer_ops {
    const char *type_name;
    struct picomesh_void_ptr_result (*create)(struct picomesh_engine *engine,
                                              const struct yconfig_node *config);
    /* `endpoint` is the dotted service.class.method; `args` the positional /_rpc
     * args (may be NULL); `jwt` the verified token or NULL for anonymous. */
    struct picomesh_authz_decision (*authorize)(void *state, const char *endpoint,
                                                const struct yjson_value *args, const char *jwt);
    void (*destroy)(void *state);
};

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_AUTHORIZERS_BASE_H */
