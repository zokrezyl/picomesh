/* secret — single source of truth for the mesh JWT signing key + TTL.
 *
 * The key is read from config at `security.jwt_secret` (env-substituted, e.g.
 * "${PICOMESH_JWT_SECRET}"). It is REQUIRED when security is in use: a missing
 * or empty value is a loud error, never a silent default — a predictable
 * signing key would let any client forge a JWT. The split mesh spawns children
 * inheriting the parent's environment, so an env-backed secret reaches every
 * node that signs or verifies. */

#ifndef PICOMESH_YSECURITY_SECRET_H
#define PICOMESH_YSECURITY_SECRET_H

#include <stdint.h>

#include <picomesh/ycore/result.h>
#include <picomesh/ysecurity/jwt.h>

#ifdef __cplusplus
extern "C" {
#endif

struct picomesh_engine;
struct yheaders;

/* Backend-side: extract the caller's verified auth context from the request
 * headers. Verifies the JWT the gateway placed in yheaders["jwt"] against the
 * shared signing secret (so a backend trusts claims by signature, not just the
 * gateway's word). If no JWT is present, falls back to the gateway-set
 * yheaders["uid"] with authenticated=0 (trusted-uid, no verifiable groups).
 * Always returns ok; `out` is populated accordingly. */
struct picomesh_void_result picomesh_authctx_from_headers(struct yheaders *hdrs,
                                                          struct picomesh_engine *engine,
                                                          struct picomesh_authctx *out);

/* OK value is a malloc'd NUL-terminated secret (caller frees). Error if the
 * engine has no config or `security.jwt_secret` is missing/empty. */
struct picomesh_string_result picomesh_security_jwt_secret(struct picomesh_engine *engine);

/* Access-token lifetime in seconds. Reads `security.access_ttl_seconds`,
 * defaulting to 900 (15 min) when absent. */
int64_t picomesh_security_access_ttl(struct picomesh_engine *engine);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_YSECURITY_SECRET_H */
