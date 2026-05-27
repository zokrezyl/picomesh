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
    static uint8_t body[BUF_MAX];
    static uint8_t resp[BUF_MAX];

    for (;;) {
        uint32_t header = 0, body_len = 0;
        if (rd(ud, &header, 4) != 4) return;
        if (rd(ud, &body_len, 4) != 4) return;
        if (body_len > BUF_MAX) return;
        if (body_len && rd(ud, body, body_len) != body_len) return;

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
        default:
            ywarn("unknown op=%u", op);
            break;
        }

        if (wr(ud, &resp_len, 4) != 4) return;
        if (resp_len && wr(ud, resp, resp_len) != resp_len) return;
    }
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

struct rpc_session {
    int fd;
    struct remote_id_entry *remote_ids;
    struct translated_class *translated;
};

struct rpc_session *rpc_session_create(int fd)
{
    struct rpc_session *s = calloc(1, sizeof(*s));
    if (s) s->fd = fd;
    return s;
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

size_t rpc_call(struct rpc_session *s, enum rpc_op op, uint32_t id, const void *body,
                size_t body_len, void *resp, size_t resp_max)
{
    if (!s) return 0;
    uint32_t header = RPC_HDR_MAKE(op, id);
    ydebug("op=%u id=%u body_len=%zu", op, id, body_len);

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
