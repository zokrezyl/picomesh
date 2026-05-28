/* RPC runtime — packed-header wire, op enum, uthash translations.
 *
 * I/O is still blocking POSIX read/write here. The async-aware variant
 * that yields the calling coroutine on EAGAIN comes in the next layer
 * (see src/yaafc/yco/) — this file stays portable. */

#include <yaafc/yclass/rpc.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>

#include <uthash.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_OBJECTS 256
#define BUF_MAX 65536

struct object_entry {
    uint64_t handle;
    void *ptr;
};

struct skel_lookup_node {
    skel_lookup_fn fn;
    struct skel_lookup_node *next;
};

struct skel_cache_entry {
    method_slot slot;
    rpc_skel_fn fn;
    UT_hash_handle hh;
};

struct rpc_server_state {
    struct object_entry objects[MAX_OBJECTS];
    size_t object_count;
    uint64_t next_handle;

    struct skel_lookup_node *lookup_chain;
    struct skel_cache_entry *skel_cache;
};

static struct rpc_server_state *server(void)
{
    static struct rpc_server_state s = {0};
    return &s;
}

void rpc_init(void)
{
    struct rpc_server_state *s = server();
    s->object_count = 0;
    s->next_handle = 1;
}

void rpc_add_skel_lookup(skel_lookup_fn fn)
{
    if (!fn) return;
    struct skel_lookup_node *node = calloc(1, sizeof(*node));
    if (!node) return;
    struct rpc_server_state *s = server();
    node->fn = fn;
    node->next = s->lookup_chain;
    s->lookup_chain = node;
}

rpc_skel_fn rpc_skel_for(method_slot slot)
{
    struct rpc_server_state *s = server();
    if (slot == METHOD_SLOT_UNDEFINED) return NULL;
    struct skel_cache_entry *e = NULL;
    HASH_FIND(hh, s->skel_cache, &slot, sizeof(slot), e);
    if (e) return e->fn;

    rpc_skel_fn fn = NULL;
    for (struct skel_lookup_node *n = s->lookup_chain; n; n = n->next) {
        fn = n->fn(slot);
        if (fn) break;
    }
    if (!fn) return NULL;

    e = calloc(1, sizeof(*e));
    if (!e) return fn;
    e->slot = slot;
    e->fn = fn;
    HASH_ADD(hh, s->skel_cache, slot, sizeof(slot), e);
    return fn;
}

uint64_t rpc_register_object(void *obj)
{
    struct rpc_server_state *s = server();
    if (s->object_count >= MAX_OBJECTS) return 0;
    uint64_t h = s->next_handle++;
    s->objects[s->object_count].handle = h;
    s->objects[s->object_count].ptr = obj;
    s->object_count++;
    return h;
}

void *rpc_handle_resolve(uint64_t h)
{
    if (!h) return NULL;
    struct rpc_server_state *s = server();
    for (size_t i = 0; i < s->object_count; ++i) {
        if (s->objects[i].handle == h) return s->objects[i].ptr;
    }
    return NULL;
}

/* Per-request caller context — populated by the yhttp /_rpc handler
 * from HTTP headers before invoking a skel. Cooperative-coroutine
 * safe so long as skels don't yield between reading these and
 * starting their work (they read at the top, then run). */
static __thread uint32_t g_caller_uid = 0;
static __thread uint32_t g_caller_sid = 0;

void rpc_set_current_caller(uint32_t uid, uint32_t sid)
{
    g_caller_uid = uid;
    g_caller_sid = sid;
}

void rpc_current_caller(uint32_t *out_uid, uint32_t *out_sid)
{
    if (out_uid) *out_uid = g_caller_uid;
    if (out_sid) *out_sid = g_caller_sid;
}

/* Drop a handle from the table and free the underlying object. Used by
 * RPC_OP_DESTROY so server-side proxy objects don't accumulate when
 * the client releases its proxy. Returns 1 if a handle was removed. */
static int rpc_handle_release(uint64_t h)
{
    if (!h) return 0;
    struct rpc_server_state *s = server();
    for (size_t i = 0; i < s->object_count; ++i) {
        if (s->objects[i].handle == h) {
            free(s->objects[i].ptr);
            /* Compact: swap with the tail. The order isn't meaningful. */
            s->objects[i] = s->objects[s->object_count - 1];
            s->object_count--;
            return 1;
        }
    }
    return 0;
}

static size_t handle_destroy(const void *body, size_t body_len, void *resp, size_t resp_max)
{
    if (body_len < sizeof(uint64_t) || resp_max < 1) return 0;
    uint64_t h;
    memcpy(&h, body, sizeof(h));
    int removed = rpc_handle_release(h);
    ((uint8_t *)resp)[0] = removed ? 1 : 0;
    return 1;
}

/* Forward declarations for the admin op handlers, plus a unified
 * dispatcher used by the HTTP /_rpc endpoint. */
static size_t handle_resolve_slot(const void *body, size_t body_len, void *resp, size_t resp_max);
static size_t handle_get_class(const void *body, size_t body_len, void *resp, size_t resp_max);
static size_t handle_create(const void *body, size_t body_len, void *resp, size_t resp_max);

size_t rpc_handle_admin_op(enum rpc_op op,
                           const void *body, size_t body_len,
                           void *resp, size_t resp_max)
{
    switch (op) {
    case RPC_OP_RESOLVE_SLOT: return handle_resolve_slot(body, body_len, resp, resp_max);
    case RPC_OP_GET_CLASS:    return handle_get_class(body, body_len, resp, resp_max);
    case RPC_OP_CREATE:       return handle_create(body, body_len, resp, resp_max);
    case RPC_OP_DESTROY:      return handle_destroy(body, body_len, resp, resp_max);
    default:                  return 0;
    }
}

struct fd_io {
    int fd;
};

static size_t fd_read(void *ud, void *buf, size_t n)
{
    struct fd_io *io = ud;
    char *p = buf;
    size_t left = n;
    while (left) {
        ssize_t r = read(io->fd, p, left);
        if (r > 0) { p += r; left -= (size_t)r; continue; }
        if (r < 0 && errno == EINTR) continue;
        return 0;
    }
    return n;
}

static size_t fd_write(void *ud, const void *buf, size_t n)
{
    struct fd_io *io = ud;
    const char *p = buf;
    size_t left = n;
    while (left) {
        ssize_t w = write(io->fd, p, left);
        if (w > 0) { p += w; left -= (size_t)w; continue; }
        if (w < 0 && errno == EINTR) continue;
        return 0;
    }
    return n;
}

static int read_full(int fd, void *buf, size_t n)
{
    struct fd_io io = {.fd = fd};
    return fd_read(&io, buf, n) == n ? 0 : -1;
}

static int write_full(int fd, const void *buf, size_t n)
{
    struct fd_io io = {.fd = fd};
    return fd_write(&io, buf, n) == n ? 0 : -1;
}

static size_t handle_resolve_slot(const void *body, size_t body_len, void *resp, size_t resp_max)
{
    char name[128];
    size_t n = body_len < sizeof(name) - 1 ? body_len : sizeof(name) - 1;
    memcpy(name, body, n);
    name[n] = 0;
    struct method_slot_result sr = method_slot_by_qname(name);
    uint32_t out;
    if (YAAFC_IS_ERR(sr)) {
        yaafc_error_destroy(sr.error);
        out = UINT32_MAX;
    } else {
        out = (uint32_t)sr.value;
    }
    if (resp_max < sizeof(out)) return 0;
    memcpy(resp, &out, sizeof(out));
    ydebug("resolve_slot('%s') -> %u", name, out);
    return sizeof(out);
}

struct get_class_ctx {
    uint8_t *out;
    size_t off;
    size_t cap;
};

static void get_class_emit(const char *name, method_slot slot, void *ud)
{
    struct get_class_ctx *gc = ud;
    size_t name_len = strlen(name);
    size_t need = 2 + name_len + 4;
    if (gc->off + need > gc->cap) return;
    uint16_t nl = (uint16_t)name_len;
    memcpy(gc->out + gc->off, &nl, 2);            gc->off += 2;
    memcpy(gc->out + gc->off, name, name_len);    gc->off += name_len;
    uint32_t rid = (uint32_t)slot;
    memcpy(gc->out + gc->off, &rid, 4);           gc->off += 4;
}

static size_t handle_get_class(const void *body, size_t body_len, void *resp, size_t resp_max)
{
    char name[128];
    size_t n = body_len < sizeof(name) - 1 ? body_len : sizeof(name) - 1;
    memcpy(name, body, n);
    name[n] = 0;
    struct class_ptr_result cr = class_by_name(name);
    if (YAAFC_IS_ERR(cr)) {
        yaafc_error_print(stderr, "[server] get_class", cr.error);
        yaafc_error_destroy(cr.error);
        return 0;
    }
    struct get_class_ctx gc = {resp, 0, resp_max};
    class_for_each_slot(cr.value, get_class_emit, &gc);
    ydebug("get_class('%s') -> %zu entries (%zu bytes)", name, gc.off / 6, gc.off);
    return gc.off;
}

static size_t handle_create(const void *body, size_t body_len, void *resp, size_t resp_max)
{
    char name[128];
    size_t n = body_len < sizeof(name) - 1 ? body_len : sizeof(name) - 1;
    memcpy(name, body, n);
    name[n] = 0;
    struct class_ptr_result cr = class_by_name(name);
    if (YAAFC_IS_ERR(cr)) {
        yaafc_error_print(stderr, "[server] create class_by_name", cr.error);
        yaafc_error_destroy(cr.error);
        return 0;
    }
    struct object_ptr_result orr = object_alloc(cr.value);
    if (YAAFC_IS_ERR(orr)) {
        yaafc_error_print(stderr, "[server] create object_alloc", orr.error);
        yaafc_error_destroy(orr.error);
        return 0;
    }
    uint64_t h = rpc_register_object(orr.value);
    if (resp_max < sizeof(h)) return 0;
    memcpy(resp, &h, sizeof(h));
    ydebug("create('%s') -> handle=%llu", name, (unsigned long long)h);
    return sizeof(h);
}

void rpc_server_run_io(void *ud, rpc_io_read_fn rd, rpc_io_write_fn wr)
{
    /* Per-peer buffers, not statics: multiple peer coroutines run
     * concurrently inside the same process (cooperative libco) and
     * would clobber a shared scratch buffer between yields. */
    uint8_t *body = malloc(BUF_MAX);
    uint8_t *resp = malloc(BUF_MAX);
    if (!body || !resp) { free(body); free(resp); return; }

    for (;;) {
        uint32_t header = 0, body_len = 0;
        if (rd(ud, &header, 4) != 4) goto done;
        if (rd(ud, &body_len, 4) != 4) goto done;
        if (body_len > BUF_MAX) goto done;
        if (body_len && rd(ud, body, body_len) != body_len) goto done;

        enum rpc_op op = RPC_HDR_OP(header);
        uint32_t id = RPC_HDR_ID(header);
        uint32_t resp_len = 0;

        switch (op) {
        case RPC_OP_CALL: {
            rpc_skel_fn fn = rpc_skel_for((method_slot)id);
            if (fn) {
                ydebug("CALL slot=%u body_len=%u", id, body_len);
                resp_len = (uint32_t)fn(body, body_len, resp, BUF_MAX);
            } else {
                ywarn("CALL slot=%u — no skel", id);
            }
            break;
        }
        case RPC_OP_RESOLVE_SLOT:
            resp_len = (uint32_t)handle_resolve_slot(body, body_len, resp, BUF_MAX);
            break;
        case RPC_OP_GET_CLASS:
            resp_len = (uint32_t)handle_get_class(body, body_len, resp, BUF_MAX);
            break;
        case RPC_OP_CREATE:
            resp_len = (uint32_t)handle_create(body, body_len, resp, BUF_MAX);
            break;
        case RPC_OP_DESTROY:
            resp_len = (uint32_t)handle_destroy(body, body_len, resp, BUF_MAX);
            break;
        default:
            ywarn("unknown op=%u", op);
            break;
        }

        if (wr(ud, &resp_len, 4) != 4) goto done;
        if (resp_len && wr(ud, resp, resp_len) != resp_len) goto done;
    }
done:
    free(body);
    free(resp);
}

void rpc_server_run(int fd)
{
    struct fd_io io = {.fd = fd};
    rpc_server_run_io(&io, fd_read, fd_write);
}

/* -------- client session ------------------------------------------- */

struct translated_class {
    char *name;
    UT_hash_handle hh;
};

struct remote_id_entry {
    method_slot local_slot;
    uint32_t remote_id;
    UT_hash_handle hh;
};

enum rpc_session_mode {
    RPC_MODE_TCP = 0,   /* legacy yrpc binary on the bare fd */
    RPC_MODE_HTTP = 1,  /* HTTP envelope, auth via headers */
};

struct rpc_session {
    int fd;
    enum rpc_session_mode mode;
    char *http_host;     /* Host header, RPC_MODE_HTTP only */
    uint32_t auth_uid;   /* X-Yaafc-Uid for HTTP requests */
    uint32_t auth_sid;   /* X-Yaafc-Sid for HTTP requests */
    struct remote_id_entry *remote_ids;
    struct translated_class *translated;
};

struct rpc_session *rpc_session_create(int fd)
{
    struct rpc_session *s = calloc(1, sizeof(*s));
    if (s) { s->fd = fd; s->mode = RPC_MODE_TCP; }
    return s;
}

struct rpc_session *rpc_session_create_http(int fd, const char *host)
{
    struct rpc_session *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->fd = fd;
    s->mode = RPC_MODE_HTTP;
    s->http_host = strdup(host ? host : "127.0.0.1");
    return s;
}

void rpc_session_set_auth(struct rpc_session *s, uint32_t uid, uint32_t sid)
{
    if (!s) return;
    s->auth_uid = uid;
    s->auth_sid = sid;
}

void rpc_session_destroy(struct rpc_session *s)
{
    if (!s) return;
    struct translated_class *cur, *tmp;
    HASH_ITER(hh, s->translated, cur, tmp) {
        HASH_DEL(s->translated, cur);
        free(cur->name);
        free(cur);
    }
    struct remote_id_entry *rcur, *rtmp;
    HASH_ITER(hh, s->remote_ids, rcur, rtmp) {
        HASH_DEL(s->remote_ids, rcur);
        free(rcur);
    }
    free(s->http_host);
    if (s->fd >= 0) close(s->fd);
    free(s);
}

uint32_t rpc_session_remote_id(struct rpc_session *s, method_slot slot)
{
    if (!s) return RPC_REMOTE_ID_UNRESOLVED;
    struct remote_id_entry *e = NULL;
    HASH_FIND(hh, s->remote_ids, &slot, sizeof(slot), e);
    return e ? e->remote_id : RPC_REMOTE_ID_UNRESOLVED;
}

void rpc_session_set_remote_id(struct rpc_session *s, method_slot slot, uint32_t remote_id)
{
    if (!s) return;
    struct remote_id_entry *e = NULL;
    HASH_FIND(hh, s->remote_ids, &slot, sizeof(slot), e);
    if (!e) {
        e = calloc(1, sizeof(*e));
        if (!e) return;
        e->local_slot = slot;
        HASH_ADD(hh, s->remote_ids, local_slot, sizeof(method_slot), e);
    }
    e->remote_id = remote_id;
}

/* Send an HTTP-wrapped RPC and parse the response body. The HTTP
 * request shape is:
 *
 *   POST /_rpc?op=<op>&id=<id> HTTP/1.1\r\n
 *   Host: <host>\r\n
 *   Content-Length: <body_len>\r\n
 *   Content-Type: application/octet-stream\r\n
 *   X-Yaafc-Uid: <uid>\r\n
 *   X-Yaafc-Sid: <sid>\r\n
 *   Connection: keep-alive\r\n
 *   \r\n
 *   <body_bytes>
 *
 * Response is HTTP/1.1 with a Content-Length-framed binary body —
 * exactly the same shape as the legacy yrpc payload would be. */
static size_t rpc_call_http(struct rpc_session *s, enum rpc_op op, uint32_t id,
                            const void *body, size_t body_len,
                            void *resp, size_t resp_max)
{
    char hdr[512];
    int hn = snprintf(hdr, sizeof(hdr),
        "POST /_rpc?op=%u&id=%u HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: application/octet-stream\r\n"
        "X-Yaafc-Uid: %u\r\n"
        "X-Yaafc-Sid: %u\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        (unsigned)op, id,
        s->http_host ? s->http_host : "127.0.0.1",
        body_len, s->auth_uid, s->auth_sid);
    if (hn <= 0 || (size_t)hn >= sizeof(hdr)) return 0;
    if (write_full(s->fd, hdr, (size_t)hn) < 0) return 0;
    if (body_len && write_full(s->fd, body, body_len) < 0) return 0;

    /* Parse the response. We only need the status line + Content-Length;
     * skip everything else. Read byte-by-byte until \r\n\r\n. */
    char line[1024];
    size_t li = 0;
    int got_blank = 0;
    int saw_cr = 0;
    int blank_state = 0; /* 0=line, 1=after \r, 2=after \r\n, 3=after \r\n\r */
    size_t content_length = 0;
    int status = 0;
    int parsed_status = 0;
    for (;;) {
        char c;
        if (read_full(s->fd, &c, 1) < 0) return 0;
        if (c != '\n') {
            if (li + 1 < sizeof(line)) line[li++] = c;
        }
        if (saw_cr && c == '\n') {
            line[li - 1] = 0; /* drop the \r */
            if (li == 1) {
                got_blank = 1;
                break;
            }
            if (!parsed_status) {
                /* "HTTP/1.1 200 OK" — pull the int */
                const char *p = strchr(line, ' ');
                if (p) status = atoi(p + 1);
                parsed_status = 1;
            } else {
                /* Looking for Content-Length: */
                if (strncasecmp(line, "Content-Length:", 15) == 0) {
                    content_length = (size_t)strtoull(line + 15, NULL, 10);
                }
            }
            li = 0;
            saw_cr = 0;
            blank_state = 2;
            continue;
        }
        saw_cr = (c == '\r');
        (void)blank_state;
    }
    if (!got_blank) return 0;
    if (status < 200 || status >= 300) {
        /* Drain the body so the connection stays usable. */
        size_t remain = content_length;
        uint8_t drain[256];
        while (remain) {
            size_t chunk = remain > sizeof(drain) ? sizeof(drain) : remain;
            if (read_full(s->fd, drain, chunk) < 0) return 0;
            remain -= chunk;
        }
        return 0;
    }
    if (content_length > resp_max) {
        /* Drain & give up — caller's buffer is too small. */
        size_t remain = content_length;
        uint8_t drain[256];
        while (remain) {
            size_t chunk = remain > sizeof(drain) ? sizeof(drain) : remain;
            if (read_full(s->fd, drain, chunk) < 0) return 0;
            remain -= chunk;
        }
        return 0;
    }
    if (content_length && read_full(s->fd, resp, content_length) < 0) return 0;
    return content_length;
}

size_t rpc_call(struct rpc_session *s, enum rpc_op op, uint32_t id, const void *body,
                size_t body_len, void *resp, size_t resp_max)
{
    if (!s) return 0;
    ydebug("mode=%d op=%u id=%u body_len=%zu", s->mode, op, id, body_len);

    if (s->mode == RPC_MODE_HTTP) {
        return rpc_call_http(s, op, id, body, body_len, resp, resp_max);
    }

    uint32_t header = RPC_HDR_MAKE(op, id);
    uint32_t bl = (uint32_t)body_len;
    if (write_full(s->fd, &header, 4) < 0) return 0;
    if (write_full(s->fd, &bl, 4) < 0) return 0;
    if (body_len && write_full(s->fd, body, body_len) < 0) return 0;

    uint32_t resp_len = 0;
    if (read_full(s->fd, &resp_len, 4) < 0) return 0;
    if (resp_len > resp_max) {
        uint8_t drain[256];
        size_t remain = resp_len;
        while (remain) {
            size_t chunk = remain > sizeof(drain) ? sizeof(drain) : remain;
            if (read_full(s->fd, drain, chunk) < 0) return 0;
            remain -= chunk;
        }
        return 0;
    }
    if (resp_len && read_full(s->fd, resp, resp_len) < 0) return 0;
    return resp_len;
}

uint32_t rpc_session_ensure_remote_id(struct rpc_session *s, method_slot local_slot)
{
    if (!s) return RPC_REMOTE_ID_UNRESOLVED;
    uint32_t cached = rpc_session_remote_id(s, local_slot);
    if (cached != RPC_REMOTE_ID_UNRESOLVED) return cached;

    struct const_char_ptr_result nr = method_slot_name(local_slot);
    if (YAAFC_IS_ERR(nr)) {
        yaafc_error_destroy(nr.error);
        return RPC_REMOTE_ID_UNRESOLVED;
    }
    const char *name = nr.value;

    uint32_t remote = RPC_REMOTE_ID_UNRESOLVED;
    size_t n =
        rpc_call(s, RPC_OP_RESOLVE_SLOT, 0, name, strlen(name), &remote, sizeof(remote));
    if (n != sizeof(remote) || remote == RPC_REMOTE_ID_UNRESOLVED) return RPC_REMOTE_ID_UNRESOLVED;

    rpc_session_set_remote_id(s, local_slot, remote);
    ydebug("lazy resolve '%s' local=%u remote=%u", name, local_slot, remote);
    return remote;
}

int rpc_session_translate_class(struct rpc_session *s, const char *class_name)
{
    if (!s || !class_name) return -1;
    struct translated_class *t = NULL;
    HASH_FIND_STR(s->translated, class_name, t);
    if (t) return 0;

    uint8_t buf[BUF_MAX];
    size_t name_len = strlen(class_name);
    size_t resp_len =
        rpc_call(s, RPC_OP_GET_CLASS, 0, class_name, name_len, buf, sizeof(buf));
    if (resp_len == 0) return -1;

    size_t off = 0;
    while (off + 2 + 4 <= resp_len) {
        uint16_t nl;
        memcpy(&nl, buf + off, 2);
        off += 2;
        if (off + nl + 4 > resp_len) break;
        char slot_name[128];
        size_t copy = nl < sizeof(slot_name) - 1 ? nl : sizeof(slot_name) - 1;
        memcpy(slot_name, buf + off, copy);
        slot_name[copy] = 0;
        off += nl;
        uint32_t rid;
        memcpy(&rid, buf + off, 4);
        off += 4;

        struct method_slot_result lr = method_slot_by_qname(slot_name);
        if (YAAFC_IS_OK(lr)) {
            rpc_session_set_remote_id(s, lr.value, rid);
            ydebug("xlat['%s'] local=%u remote=%u", slot_name, lr.value, rid);
        } else {
            yaafc_error_destroy(lr.error);
        }
    }

    t = calloc(1, sizeof(*t));
    if (!t) return 0;
    t->name = strdup(class_name);
    if (!t->name) {
        free(t);
        return 0;
    }
    HASH_ADD_KEYPTR(hh, s->translated, t->name, strlen(t->name), t);
    return 0;
}
