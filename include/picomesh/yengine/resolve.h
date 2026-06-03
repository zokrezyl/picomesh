#ifndef PICOMESH_YENGINE_RESOLVE_H
#define PICOMESH_YENGINE_RESOLVE_H

#include <picomesh/yclass/class.h> /* struct ctx, struct object */
#include <picomesh/ycore/result.h>

#ifdef __cplusplus
extern "C" {
#endif

struct picomesh_engine;
struct jinvoke_params;

/* Everything a transport needs to dispatch a "service.class.method" call,
 * AFTER the active-service gate has passed. The receiver object is acquired
 * against `ctx`; hand the whole struct to picomesh_service_call_release when
 * done (it releases the object against the same ctx).
 *
 * `method_qname` is the underscore-joined path — identical to the codegen's
 * qualified slot name, and therefore the jinvoke / minvoke lookup key. The
 * gate guarantees it names a method of an ACTIVE service on this node, so a
 * transport may look the typed leaf up directly with it. */
struct picomesh_service_call {
    struct ctx ctx;                      /* peer set if the service is remote */
    struct object *obj;                  /* acquired receiver (local or proxy) */
    char class_qname[160];
    char method_qname[192];
    const struct jinvoke_params *params; /* call-signature metadata, may be NULL */
};

PICOMESH_RESULT_DECLARE(picomesh_service_call, struct picomesh_service_call);

/* Resolve + gate a dotted `service.class.method` path against the ACTIVE
 * service model for this node, returning everything a transport needs to
 * dispatch. Shared by every inbound service-path transport (the gateway's
 * /_rpc, the msgpack frontend, …) so the active-service gate lives in exactly
 * one place and no such transport reaches a global method table without it.
 *
 * "Active" means: a configured remote for that service (reachable over a
 * peer) OR a plugin activated in this process (registration == activation in
 * this runtime, so a registered class proves it). A path naming an inactive
 * service is rejected before any class/method lookup.
 *
 * The error message is the contract a transport maps to a wire code:
 *   "resolve: bad path …"           malformed (want service.class.method)
 *   "resolve: service not active …" not an activated plugin or remote here
 *   "resolve: no such class …"      the receiver class is not registered/reachable
 */
struct picomesh_service_call_result
picomesh_resolve_service_call(struct picomesh_engine *engine, const char *path);

/* Release the receiver object acquired by picomesh_resolve_service_call.
 * Safe on a zeroed/already-released call. */
void picomesh_service_call_release(struct picomesh_service_call *call);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_YENGINE_RESOLVE_H */
