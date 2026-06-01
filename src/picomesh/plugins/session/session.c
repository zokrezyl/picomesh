/* session — session-id ↔ user/token mapping.
 *
 * Scenario shape:
 *   start(user_id, provider_id)  → opaque session token string (mints, stores)
 *   lookup(token)                → uint32 user_id (0 if absent / expired)
 *   destroy(token)               → 1 if removed, 0 if unknown
 *   count_active                 → number of sessions live now
 *
 * The token is a 128-bit random value (hex). It is an OPAQUE bearer secret:
 * unguessable, never logged, never derived from anything client-supplied.
 *
 * All state lives in the storage backend reached via the engine's
 * `storage` remote. Key layout in the `session` context:
 *
 *   count          → number of live sessions.
 *   uid:<token>    → user_id bound to that token.
 *   prov:<token>   → provider_id bound to that token.
 *
 * The plugin process itself carries no in-memory bookkeeping, so a
 * crash + restart still serves the same sessions, and every remote
 * object on this service points at the same data automatically.
 */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/plugin/sharded_storage/sharded_storage.h>
#include <picomesh/yclass/rpc.h>
#include <string.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>

struct PICOMESH_CLASS_ANNOTATE("class@session:store") session_store_data {
    /* Empty — the plugin holds no per-object state. Codegen still wants
     * a struct annotated with the class accessor; a dummy byte keeps it
     * a complete type so object_alloc has something to size. */
    char _unused;
};

/* Helper: open a storage_sql proxy on the configured `storage` remote.
 * Returns a Result so callers can propagate the failure path cleanly. */
struct storage_handle {
    struct ctx c;
    struct object *obj;
};
PICOMESH_RESULT_DECLARE(session_storage_handle, struct storage_handle);

static struct session_storage_handle_result open_storage(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(session_storage_handle, "session: no active engine");
    struct storage_handle h = {.c = picomesh_engine_service_ctx(e, "sharded_storage")};
    /* peer==NULL ⇒ storage is collocated in THIS process; create resolves it
     * as a local in-process object. A non-NULL peer is the remote-mesh path.
     * Both go through sharded_storage_db_create — don't bail on a missing peer. */
    struct object_ptr_result o = sharded_storage_db_create(&h.c);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(session_storage_handle, "session: storage_db_create failed", o);
    h.obj = o.value;
    return PICOMESH_OK(session_storage_handle, h);
}

#define SESSION_CTX "session"

static void close_storage(struct storage_handle *h)
{
    /* The storage object is a cached, service-lifetime dependency
     * (rpc_object_acquire owns it); nothing to release per call. */
    (void)h;
}

/* The store holds string values; session state is integer counters and
 * ids, so serialize them as decimal strings on the way in and parse them
 * back on the way out. A failed write is propagated, never swallowed — a
 * session row that was not persisted must not look like success. */
static struct picomesh_void_result kv_set_int(struct storage_handle *h, struct yheaders *hdrs,
                                              const char *key, int64_t value)
{
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "%lld", (long long)value);
    struct picomesh_int_result r = sharded_storage_db_set(&h->c, h->obj, hdrs, SESSION_CTX, key, vbuf);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "session: storage write failed", r);
    return PICOMESH_OK_VOID();
}

/* Read an int key. A real backend error is propagated; an absent/empty key
 * (db_get returns "") yields `fallback` — the two are NOT conflated. */
static struct picomesh_int64_result kv_get_int(struct storage_handle *h, struct yheaders *hdrs,
                                               const char *key, int64_t fallback)
{
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, SESSION_CTX, key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "session: storage read failed", r);
    int64_t v = (r.value && r.value[0]) ? strtoll(r.value, NULL, 10) : fallback;
    free(r.value);
    return PICOMESH_OK(picomesh_int64, v);
}

/* Atomic counter bump — storage serializes the read-add-write so the live
 * session count never loses an update. Propagates a backend failure (the
 * OK value is the count after the add). */
static struct picomesh_int64_result kv_incr(struct storage_handle *h, struct yheaders *hdrs,
                                            const char *key, int64_t delta)
{
    struct picomesh_int64_result r = sharded_storage_db_incr(&h->c, h->obj, hdrs, SESSION_CTX, key, delta);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "session: counter update failed", r);
    return r;
}

/* Allocate a fresh session token: 128 bits of randomness, lowercase-hex
 * encoded (32 chars + NUL). Opaque and unguessable, and a 2^128 space makes
 * collisions impossible in practice.
 *
 * This replaces the old sequential `next_sid` counter, whose non-atomic
 * `get → +1 → set` over two storage RPCs (each a coroutine yield) raced under
 * concurrent logins: two users were handed the SAME id, the later `uid:<id>`
 * write clobbered the earlier, and the token then resolved to the WRONG user —
 * which made owner-checked `git_repo.store.put_file` fail under load. A random
 * token shares no counter, so there is nothing to race on.
 *
 * FAIL CLOSED: a session token is a bearer secret. If the kernel cannot
 * give us secure random bytes we refuse to mint one — never fall back to a
 * predictable clock/address-seeded PRNG. Returns 1 on success, 0 if secure
 * randomness was unavailable. */
static int alloc_token(char *out, size_t cap)
{
    uint8_t raw[16];
    size_t got = 0;
    while (got < sizeof(raw)) {
        ssize_t n = getrandom(raw + got, sizeof(raw) - got, 0);
        if (n < 0) {
            if (errno == EINTR) continue;  /* interrupted by signal → retry */
            return 0;                       /* no secure entropy → fail closed */
        }
        got += (size_t)n;
    }
    static const char hex[] = "0123456789abcdef";
    size_t k = 0;
    for (size_t i = 0; i < sizeof(raw) && k + 2 < cap; ++i) {
        out[k++] = hex[raw[i] >> 4];
        out[k++] = hex[raw[i] & 0x0f];
    }
    out[k < cap ? k : cap - 1] = 0;
    return 1;
}

PICOMESH_CLASS_ANNOTATE("override@session:store:store_start")
struct picomesh_string_result session_store_start_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                    uint32_t user_id, uint32_t provider_id)
{
    (void)ctx; (void)obj;
    struct session_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_string, "session_start: open_storage failed", sr);
    struct storage_handle h = sr.value;

    char tok[40];
    if (!alloc_token(tok, sizeof(tok)))
        return PICOMESH_ERR(picomesh_string, "session_start: secure random unavailable");

    /* Every row here is mandatory: if any write fails, the session is only
     * partially persisted, so we must NOT hand back a valid-looking token.
     * Propagate the first failure instead. */
    char k[64];
    snprintf(k, sizeof(k), "uid:%s", tok);
    struct picomesh_void_result w1 = kv_set_int(&h, hdrs, k, (int64_t)user_id);
    if (PICOMESH_IS_ERR(w1)) { close_storage(&h); return PICOMESH_ERR(picomesh_string, "session_start: persist uid failed", w1); }
    snprintf(k, sizeof(k), "prov:%s", tok);
    struct picomesh_void_result w2 = kv_set_int(&h, hdrs, k, (int64_t)provider_id);
    if (PICOMESH_IS_ERR(w2)) { close_storage(&h); return PICOMESH_ERR(picomesh_string, "session_start: persist provider failed", w2); }
    struct picomesh_int64_result wc = kv_incr(&h, hdrs, "count", 1);
    if (PICOMESH_IS_ERR(wc)) { close_storage(&h); return PICOMESH_ERR(picomesh_string, "session_start: bump count failed", wc); }

    close_storage(&h);
    /* Never log the token itself — it is the bearer secret. */
    yinfo("session: started user=%u provider=%u", user_id, provider_id);
    char *out = strdup(tok);
    if (!out) return PICOMESH_ERR(picomesh_string, "session_start: out of memory");
    return PICOMESH_OK(picomesh_string, out);
}

PICOMESH_CLASS_ANNOTATE("override@session:store:store_lookup")
struct picomesh_uint32_result session_store_lookup_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                     const char *token)
{
    (void)ctx; (void)obj;
    if (!token || !*token) return PICOMESH_OK(picomesh_uint32, 0);
    struct session_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "session_lookup: open_storage failed", sr);
    struct storage_handle h = sr.value;

    char k[64];
    snprintf(k, sizeof(k), "uid:%s", token);
    struct picomesh_int64_result uidr = kv_get_int(&h, hdrs, k, 0);
    close_storage(&h);
    if (PICOMESH_IS_ERR(uidr)) return PICOMESH_ERR(picomesh_uint32, "session_lookup: read failed", uidr);
    return PICOMESH_OK(picomesh_uint32, (uint32_t)uidr.value);
}

PICOMESH_CLASS_ANNOTATE("override@session:store:store_destroy")
struct picomesh_int_result session_store_destroy_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                   const char *token)
{
    (void)ctx; (void)obj;
    if (!token || !*token) return PICOMESH_OK(picomesh_int, 0);
    struct session_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "session_destroy: open_storage failed", sr);
    struct storage_handle h = sr.value;

    /* db_del is the atomic point: MDBX serializes the delete on the shard,
     * so exactly ONE of two concurrent destroys of the same token sees
     * removed==1. Gate the count decrement on that → idempotent, no
     * double-decrement (the old exists-then-decrement raced). */
    char k[64];
    snprintf(k, sizeof(k), "uid:%s", token);
    struct picomesh_int_result del = sharded_storage_db_del(&h.c, h.obj, hdrs, SESSION_CTX, k);
    if (PICOMESH_IS_ERR(del)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "session_destroy: delete failed", del); }
    if (del.value == 0) { close_storage(&h); return PICOMESH_OK(picomesh_int, 0); }  /* unknown / already gone */

    snprintf(k, sizeof(k), "prov:%s", token);
    struct picomesh_int_result pdel = sharded_storage_db_del(&h.c, h.obj, hdrs, SESSION_CTX, k);
    if (PICOMESH_IS_ERR(pdel)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "session_destroy: delete prov failed", pdel); }
    struct picomesh_int64_result dc = kv_incr(&h, hdrs, "count", -1);
    if (PICOMESH_IS_ERR(dc)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "session_destroy: count update failed", dc); }

    close_storage(&h);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@session:store:store_count_active")
struct picomesh_size_result session_store_count_active_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct session_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "session_count: open_storage failed", sr);
    struct storage_handle h = sr.value;
    struct picomesh_int64_result cr = kv_get_int(&h, hdrs, "count", 0);
    close_storage(&h);
    if (PICOMESH_IS_ERR(cr)) return PICOMESH_ERR(picomesh_size, "session_count: read failed", cr);
    return PICOMESH_OK(picomesh_size, (size_t)(cr.value < 0 ? 0 : cr.value));
}

#include "session.gen.c"
