/* jinvoke — JSON-aware method invocation.
 *
 * Parallel to `rpc_skel_fn` (binary wire dispatch). Each annotated
 * method gets a JSON invoker emitted by codegen alongside its binary
 * skel: the invoker reads positional args from a `yjson_value` array,
 * calls the local impl, and writes the result into a `yjson_writer`.
 *
 * The yttp / cli frontends use this. The binary RPC layer keeps
 * using `rpc_skel_fn`. */

#ifndef YAAFC_YCLASS_JINVOKE_H
#define YAAFC_YCLASS_JINVOKE_H

#include <yaafc/yjson/yjson.h>

#ifdef __cplusplus
extern "C" {
#endif

struct object;

/* Invoker signature. `args` is a JSON array (positional). `obj` is
 * the already-allocated instance (must match the slot's owning
 * class). On success, the invoker pushes ONE value (the method's
 * return) into `result`. On failure it writes a diagnostic into
 * `err_msg` (a NUL-terminated buffer of `err_cap` bytes) and returns
 * non-zero. */
typedef int (*jinvoke_fn)(struct object *obj, const struct yjson_value *args,
                          struct yjson_writer *result, char *err_msg, size_t err_cap);

typedef jinvoke_fn (*jinvoke_lookup_fn)(const char *qname);
void jinvoke_add_lookup(jinvoke_lookup_fn fn);
jinvoke_fn jinvoke_for(const char *qname);

#ifdef __cplusplus
}
#endif

#endif /* YAAFC_YCLASS_JINVOKE_H */
