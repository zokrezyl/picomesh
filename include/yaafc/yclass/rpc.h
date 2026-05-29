/* Tiny RPC runtime.
 *
 * Wire request:
 *   u32 header     — bits 31:28 = enum rpc_op, bits 27:0 = id
 *                    (id is the slot index for RPC_OP_CALL, 0 otherwise)
 *   u32 body_len
 *   u8  body[body_len]
 *
 * For RPC_OP_CALL, the body shape is:
 *   u32 caller_uid    — auth context propagated across the wire.
 *   u32 caller_sid    — caller's session id (0 if anonymous).
 *   ... packed args follow ...
 *
 * Wire response:
 *   u32 resp_len
 *   u8  resp[resp_len]
 *
 * For RPC_OP_CALL skel responses, the first response byte is a status:
 *   0 = ok       → followed by sizeof(value_type) value bytes.
 *   1 = error    → followed by u32 msg_len + msg_bytes. The client side
 *                  rebuilds a `yaafc_error` with that message so the
 *                  caller's chain doesn't collapse to a generic string.
 *
 * Argument types on the wire:
 *   - Scalar PODs (uint32, int64, size_t, etc.) → memcpy'd in place.
 *   - Strings (`const char *`) → u32 length + UTF-8 bytes.
 *   - Richer shapes (records, lists) → carried as a string in a
 *     well-known encoding (JSON / msgpack) chosen by the impl.
 *     Anything that wants strong typing across the wire today should
 *     hand-roll its pack/unpack via the string slot; the codegen
 *     supports this without runtime changes.
 *
 * Admin and call live in different op-spaces — the slot id never collides
 * with a sentinel. */

#ifndef YAAFC_YCLASS_RPC_H
#define YAAFC_YCLASS_RPC_H

#include <yaafc/yclass/class.h>

#include <stddef.h>
#include <stdint.h>

struct rpc_session;

/* Wire-bytes → typed-call bridge for one slot. */
typedef size_t (*rpc_skel_fn)(const void *body, size_t body_len, void *resp, size_t resp_max);

enum rpc_op {
    RPC_OP_CALL = 0,        /* id = slot index; body = packed args */
    RPC_OP_RESOLVE_SLOT,    /* body = slot name; resp = u32 server slot id */
    RPC_OP_GET_CLASS,       /* body = class name; resp = (u16 nl, name, u32 id)* */
    RPC_OP_CREATE,          /* body = class name; resp = u64 handle */
    RPC_OP_DESTROY,         /* body = u64 handle; resp = u8 (1 if removed) */
};

#define RPC_OP_SHIFT 28
#define RPC_OP_MASK 0xFu
#define RPC_ID_MASK 0x0FFFFFFFu
#define RPC_HDR_MAKE(op, id) (((uint32_t)(op) << RPC_OP_SHIFT) | ((id) & RPC_ID_MASK))
#define RPC_HDR_OP(h) ((enum rpc_op)(((h) >> RPC_OP_SHIFT) & RPC_OP_MASK))
#define RPC_HDR_ID(h) ((h) & RPC_ID_MASK)

/* ---- Server side -------------------------------------------------- */

void rpc_init(void);
uint64_t rpc_register_object(void *obj);
void *rpc_handle_resolve(uint64_t handle);

/* Read/write callbacks supplied by the transport. The blocking-fd variant
 * uses POSIX read()/write(); the yloop variant uses yloop_read/yloop_write
 * (which yields the coroutine). Both should return n on success, 0 on EOF
 * or unrecoverable error. */
typedef size_t (*rpc_io_read_fn)(void *ud, void *buf, size_t n);
typedef size_t (*rpc_io_write_fn)(void *ud, const void *buf, size_t n);

void rpc_server_run_io(void *ud, rpc_io_read_fn rd, rpc_io_write_fn wr);

/* Blocking POSIX fd convenience wrapper. */
void rpc_server_run(int fd);

typedef rpc_skel_fn (*skel_lookup_fn)(method_slot slot);
void rpc_add_skel_lookup(skel_lookup_fn fn);
rpc_skel_fn rpc_skel_for(method_slot slot);

/* Auth-context plumbing for the HTTP /_rpc boundary. The yhttp
 * handler reads X-Yaafc-Uid / -Sid headers and calls
 * rpc_set_current_caller before invoking the skel; the skel's
 * generated prologue picks the values up via rpc_current_caller.
 * Thread-local storage; safe across one coroutine's run. */
void rpc_set_current_caller(uint32_t uid, uint32_t sid);
void rpc_current_caller(uint32_t *out_uid, uint32_t *out_sid);

/* Unified admin-op dispatch — used by the HTTP /_rpc handler to
 * route RPC_OP_RESOLVE_SLOT / GET_CLASS / CREATE / DESTROY without
 * duplicating the case statement. */
size_t rpc_handle_admin_op(enum rpc_op op,
                           const void *body, size_t body_len,
                           void *resp, size_t resp_max);

/* ---- Client side -------------------------------------------------- */

/* Raw-binary transport (legacy yrpc): each rpc_call writes header,
 * body_len, body to the fd; reads u32 resp_len + bytes. */
struct rpc_session *rpc_session_create(int fd);

/* HTTP transport: each rpc_call is wrapped in a POST /_rpc?op=N&id=N
 * HTTP/1.1 request. Auth context flows via HTTP headers
 *   X-Yaafc-Uid: <n>
 *   X-Yaafc-Sid: <n>
 * (yaapp's model — the gateway is the auth boundary, and "headers"
 * are the natural carrier for request-scoped context like uid/sid
 * and, later, tracing ids).
 *
 * The fd must be a connected TCP socket to a yhttp server. `host` is
 * stashed as the HTTP Host header. */
struct rpc_session *rpc_session_create_http(int fd, const char *host);

void rpc_session_destroy(struct rpc_session *s);

/* Set auth headers attached to subsequent calls. Only meaningful for
 * HTTP-mode sessions; ignored otherwise. */
void rpc_session_set_auth(struct rpc_session *s, uint32_t uid, uint32_t sid);

size_t rpc_call(struct rpc_session *s, enum rpc_op op, uint32_t id, const void *body,
                size_t body_len, void *resp, size_t resp_max);

#define RPC_REMOTE_ID_UNRESOLVED UINT32_MAX

uint32_t rpc_session_remote_id(struct rpc_session *s, method_slot local_slot);
void rpc_session_set_remote_id(struct rpc_session *s, method_slot local_slot, uint32_t remote_id);

int rpc_session_translate_class(struct rpc_session *s, const char *class_name);
uint32_t rpc_session_ensure_remote_id(struct rpc_session *s, method_slot local_slot);

/* Generic, class-name-keyed object construction/teardown — the runtime
 * twin of the codegen-emitted typed `<class>_create(struct ctx *)`.
 * Lets a caller that links no typed `*_create` for the target class
 * (the gateway, forwarding to a backend) build a local instance or a
 * remote proxy purely from the qualified class name. A ctx with no
 * session yields a local object; a ctx with a session does a remote
 * RPC_OP_CREATE and wraps the handle in a proxy. Release with
 * object_release_in_ctx, which sends RPC_OP_DESTROY for proxies. */
struct object_ptr_result object_create_in_ctx(struct ctx *ctx, const char *class_qname);
void object_release_in_ctx(struct ctx *ctx, struct object *obj);

#endif /* YAAFC_YCLASS_RPC_H */
