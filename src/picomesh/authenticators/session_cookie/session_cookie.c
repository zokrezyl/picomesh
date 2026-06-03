/* `session_cookie` authenticator — resolve an opaque session id (cookie or a
 * same-named forwarded header) to a JWT via a configured `lookup` RPC, then
 * verify it.
 *
 * Config:
 *   - type: session_cookie
 *     cookie: picomesh-sid                       # cookie name
 *     header: picomesh-sid                       # optional alt header
 *     lookup: session.session.session_jwt        # RPC: sid -> JWT (required)
 *
 * The authenticator knows nothing about how sessions are stored — it calls the
 * configured lookup path and treats the result as a JWT (or a payload carrying
 * one). The sid is a bearer secret; an sid that is present but resolves to no
 * valid JWT FAILS the chain (401), it does not fall through. */

#include <picomesh/authenticators/base.h>
#include <picomesh/yengine/resolve.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/ysecurity/jwt_verifier.h>

#include "../http_util.h"
#include "../jwt_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct session_cookie_state {
    struct picomesh_engine *engine;
    const char *cookie;            /* cookie name (points into config) */
    const char *header;            /* alt header name */
    const char *lookup;            /* sid -> JWT RPC path */
    struct picomesh_jwt_verifier *verifier;
};

static struct picomesh_void_ptr_result session_cookie_create(struct picomesh_engine *engine,
                                                             const struct yconfig_node *config)
{
    const char *cookie = yconfig_node_as_string(yconfig_node_get(config, "cookie"), "picomesh-sid");
    const char *header = yconfig_node_as_string(yconfig_node_get(config, "header"), cookie);
    const char *lookup = yconfig_node_as_string(yconfig_node_get(config, "lookup"), NULL);
    if (!lookup || !*lookup)
        return PICOMESH_ERR(picomesh_void_ptr,
                            "session_cookie: `lookup` is required (e.g. session.session.session_jwt)");

    struct session_cookie_state *state = calloc(1, sizeof(*state));
    if (!state) return PICOMESH_ERR(picomesh_void_ptr, "session_cookie: out of memory");
    state->engine = engine;
    state->cookie = cookie;
    state->header = header;
    state->lookup = lookup;
    struct picomesh_void_ptr_result verifier = picomesh_jwt_verifier_create(engine);
    if (PICOMESH_IS_ERR(verifier)) { free(state); return PICOMESH_ERR(picomesh_void_ptr, "session_cookie: verifier create failed", verifier); }
    state->verifier = verifier.value;
    return PICOMESH_OK(picomesh_void_ptr, state);
}

static struct picomesh_authn_outcome fail(const char *reason)
{
    struct picomesh_authn_outcome outcome = {0};
    outcome.source = "session_cookie";
    outcome.error = strdup(reason);
    return outcome;
}

static struct picomesh_authn_outcome session_cookie_authenticate(void *state_ptr,
                                                                 const struct picomesh_authn_request *request)
{
    struct session_cookie_state *state = state_ptr;
    struct picomesh_authn_outcome outcome = {0};

    char sid[160];
    if (!authn_cookie_value(request->headers_raw, request->headers_raw_len, state->cookie, sid, sizeof(sid)) || !sid[0]) {
        if (!authn_header_value(request->headers_raw, request->headers_raw_len, state->header, sid, sizeof(sid)) || !sid[0])
            return outcome; /* no cookie/header → no match, try next */
    }

    char args[256];
    if (!authn_build_string_args(args, sizeof(args), sid)) return fail("session id too long");
    struct picomesh_string_result lookup = picomesh_engine_invoke_json(state->engine, state->lookup, args, NULL);
    if (PICOMESH_IS_ERR(lookup)) { picomesh_error_destroy(lookup.error); return fail("session lookup failed"); }

    char *jwt = authn_extract_jwt(lookup.value);
    free(lookup.value);
    if (!jwt) return fail("unknown or expired session");

    struct picomesh_string_result claims = picomesh_jwt_verifier_verify(state->verifier, jwt);
    if (PICOMESH_IS_ERR(claims)) { picomesh_error_destroy(claims.error); free(jwt); return fail("session JWT failed verification"); }
    free(claims.value);

    outcome.jwt = jwt;
    outcome.source = "session_cookie";
    return outcome;
}

static void session_cookie_destroy(void *state_ptr)
{
    struct session_cookie_state *state = state_ptr;
    if (!state) return;
    picomesh_jwt_verifier_destroy(state->verifier);
    free(state);
}

const struct picomesh_authenticator_ops *picomesh_authenticator_session_cookie_ops(void)
{
    static const struct picomesh_authenticator_ops ops = {
        .type_name = "session_cookie",
        .create = session_cookie_create,
        .authenticate = session_cookie_authenticate,
        .destroy = session_cookie_destroy,
    };
    return &ops;
}
