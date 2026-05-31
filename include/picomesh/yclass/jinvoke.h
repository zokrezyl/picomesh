/* jinvoke — JSON-aware method invocation.
 *
 * Parallel to `rpc_skel_fn` (binary wire dispatch). Each annotated
 * method gets a JSON invoker emitted by codegen alongside its binary
 * skel: the invoker reads positional args from a `yjson_value` array
 * and writes the result into a `yjson_writer`.
 *
 * The invoker forwards the caller's `struct ctx` straight into the
 * public stub, so the SAME entry point serves two callers:
 *   - JSON frontends that own the object locally (yttp / cli) pass a
 *     zeroed/NULL ctx → the stub dispatches in-process.
 *   - the gateway, which has no local plugin objects, passes a ctx
 *     whose `session` points at the owning backend → the stub packs
 *     the args to the binary wire and forwards over yrpc.
 *
 * The binary RPC layer keeps using `rpc_skel_fn`. */

#ifndef PICOMESH_YCLASS_JINVOKE_H
#define PICOMESH_YCLASS_JINVOKE_H

#include <picomesh/yjson/yjson.h>

#ifdef __cplusplus
extern "C" {
#endif

struct object;
struct ctx;
struct yheaders;

/* Invoker signature. `ctx` is the framework dispatch context (NULL/
 * zeroed → local; live `session` → forward to the owning backend).
 * `hdrs` is the request-header bag passed straight through to the stub
 * (the dispatch layer populates it from the envelope). `args` is the
 * positional JSON args array; `obj` is the target instance (local or a
 * remote proxy). On success the invoker pushes ONE value into `result`;
 * on failure it writes a diagnostic into `err_msg` (NUL-terminated, of
 * `err_cap` bytes) and returns non-zero. */
typedef int (*jinvoke_fn)(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err_msg, size_t err_cap);

typedef jinvoke_fn (*jinvoke_lookup_fn)(const char *qname);
void jinvoke_add_lookup(jinvoke_lookup_fn fn);
jinvoke_fn jinvoke_for(const char *qname);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_YCLASS_JINVOKE_H */
