/* Shared helper: extract a JWT from a lookup-service response envelope.
 *
 * A configured `lookup` RPC (session→JWT, opaque-token→JWT) returns either a
 * bare JWT string or a payload object carrying one. The generic authenticators
 * accept both shapes without knowing the service's internals. Header-only
 * (static inline) so each TU gets its own copy. */

#ifndef PICOMESH_AUTHENTICATORS_JWT_UTIL_H
#define PICOMESH_AUTHENTICATORS_JWT_UTIL_H

#include <picomesh/yjson/yjson.h>

#include <stdlib.h>
#include <string.h>

/* `envelope` is `{"result": <value>}` from picomesh_engine_invoke_json. Returns
 * a malloc'd JWT string (caller frees), or NULL if the result carries no JWT. */
static inline char *authn_extract_jwt(const char *envelope)
{
    if (!envelope) return NULL;
    struct yjson_doc *doc = yjson_parse(envelope, strlen(envelope));
    if (!doc) return NULL;
    const struct yjson_value *result = yjson_object_get(yjson_doc_root(doc), "result");
    const char *jwt = NULL;
    if (result && yjson_is_string(result)) {
        jwt = yjson_as_string(result, NULL);
    } else if (result && yjson_is_object(result)) {
        jwt = yjson_as_string(yjson_object_get(result, "jwt"), NULL);
        if (!jwt) jwt = yjson_as_string(yjson_object_get(result, "access_jwt"), NULL);
        if (!jwt) jwt = yjson_as_string(yjson_object_get(result, "access_token"), NULL);
    }
    char *out = (jwt && *jwt) ? strdup(jwt) : NULL;
    yjson_doc_free(doc);
    return out;
}

#endif /* PICOMESH_AUTHENTICATORS_JWT_UTIL_H */
