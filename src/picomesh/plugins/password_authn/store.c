/* password_authn — password verification backed by the storage service.
 *
 *   register(user_id, pw_hash)      → 1 created, 0 already-exists
 *   authenticate(user_id, pw_hash)  → 1 ok, 0 mismatch / unknown
 *   change_password(user_id, hash)  → 1 ok, 0 unknown
 *   count_registered                → size
 *
 * Passwords are compared by hash bit-for-bit. The wire format carries
 * int64; the frontend converts the user's password string to an int64
 * via a deterministic hash before calling.
 *
 * Storage layout in the `password_authn` context:
 *   hash:<uid>  → int64 hash
 *   count       → number of registered users
 */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/plugin/sharded_storage/sharded_storage.h>
#include <picomesh/yclass/rpc.h>
#include <string.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct PICOMESH_CLASS_ANNOTATE("class@password_authn:password_authn") password_authn_password_authn_data {
    char _unused;
};

struct pw_storage_handle {
    struct ctx c;
    struct object *obj;
};
PICOMESH_RESULT_DECLARE(pw_storage_handle, struct pw_storage_handle);

static struct pw_storage_handle_result open_storage(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(pw_storage_handle, "password_authn: no active engine");
    struct pw_storage_handle h = {.c = picomesh_engine_service_ctx(e, "sharded_storage")};
    /* peer==NULL ⇒ storage is collocated in THIS process; create resolves it
     * as a local in-process object. A non-NULL peer is the remote-mesh path.
     * Both go through sharded_storage_db_create — don't bail on a missing peer. */
    struct object_ptr_result o = sharded_storage_db_create(&h.c);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(pw_storage_handle, "password_authn: storage_db_create failed", o);
    h.obj = o.value;
    return PICOMESH_OK(pw_storage_handle, h);
}

#define PW_CTX "password_authn"

static void close_storage(struct pw_storage_handle *h)
{
    /* The storage object is a cached, service-lifetime dependency
     * (rpc_object_acquire owns it); nothing to release per call. */
    (void)h;
}

/* The store holds string values; the password hash and the registered
 * count are integers, so serialize as decimal strings and parse back. */
/* A failed write is propagated, never swallowed. */
static struct picomesh_void_result kv_set_int(struct pw_storage_handle *h, struct yheaders *hdrs,
                                              const char *key, int64_t value)
{
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "%lld", (long long)value);
    struct picomesh_int_result r = sharded_storage_db_set(&h->c, h->obj, hdrs, PW_CTX, key, vbuf);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "password_authn: storage write failed", r);
    return PICOMESH_OK_VOID();
}

/* Read an int. A real backend error is propagated; an absent/empty key
 * (db_get → "") yields `fallback`. */
static struct picomesh_int64_result kv_get_int(struct pw_storage_handle *h, struct yheaders *hdrs,
                                               const char *key, int64_t fallback)
{
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, PW_CTX, key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "password_authn: storage read failed", r);
    int64_t v = (r.value && r.value[0]) ? strtoll(r.value, NULL, 10) : fallback;
    free(r.value);
    return PICOMESH_OK(picomesh_int64, v);
}

/* Existence check. A real backend error is propagated; OK carries 0/1. */
static struct picomesh_int_result kv_exists(struct pw_storage_handle *h, struct yheaders *hdrs, const char *key)
{
    struct picomesh_int_result r = sharded_storage_db_exists(&h->c, h->obj, hdrs, PW_CTX, key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int, "password_authn: storage exists failed", r);
    return r;
}

/* Atomic counter bump — propagates a backend failure. */
static struct picomesh_int64_result kv_incr(struct pw_storage_handle *h, struct yheaders *hdrs,
                                            const char *key, int64_t delta)
{
    struct picomesh_int64_result r = sharded_storage_db_incr(&h->c, h->obj, hdrs, PW_CTX, key, delta);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "password_authn: counter update failed", r);
    return r;
}

PICOMESH_CLASS_ANNOTATE("override@password_authn:password_authn:password_authn_register")
struct picomesh_int_result password_authn_password_authn_register_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                           uint32_t user_id, int64_t hash)
{
    (void)ctx; (void)obj;
    struct pw_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "password_authn_register: open_storage failed", sr);
    struct pw_storage_handle h = sr.value;

    /* put_if_absent ELECTS one registrant for this uid atomically: two
     * concurrent registers can't both pass an exists-check, both store a
     * credential and both bump the count. Only the inserting caller bumps
     * the total and reports 1; a loser reports "already registered" (0). */
    char k[64];
    snprintf(k, sizeof(k), "hash:%u", user_id);
    char hbuf[32];
    snprintf(hbuf, sizeof(hbuf), "%lld", (long long)hash);
    struct picomesh_int_result ins = sharded_storage_db_put_if_absent(&h.c, h.obj, hdrs, PW_CTX, k, hbuf);
    if (PICOMESH_IS_ERR(ins)) {
        close_storage(&h);
        return PICOMESH_ERR(picomesh_int, "password_authn_register: storage write failed", ins);
    }
    if (ins.value == 0) { close_storage(&h); return PICOMESH_OK(picomesh_int, 0); }
    struct picomesh_int64_result cinc = kv_incr(&h, hdrs, "count", 1);
    if (PICOMESH_IS_ERR(cinc)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "password_authn_register: bump count failed", cinc); }
    close_storage(&h);
    yinfo("password_authn: registered uid=%u", user_id);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@password_authn:password_authn:password_authn_authenticate")
struct picomesh_int_result password_authn_password_authn_authenticate_impl(struct ctx *ctx,
                                                               struct object *obj,
                                                               struct yheaders *hdrs,
                                                               uint32_t user_id, int64_t hash)
{
    (void)ctx; (void)obj;
    struct pw_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "password_authn_authenticate: open_storage failed", sr);
    struct pw_storage_handle h = sr.value;
    char k[64];
    snprintf(k, sizeof(k), "hash:%u", user_id);
    struct picomesh_int_result ex = kv_exists(&h, hdrs, k);
    if (PICOMESH_IS_ERR(ex)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "password_authn_authenticate: existence check failed", ex); }
    if (!ex.value) { close_storage(&h); return PICOMESH_OK(picomesh_int, 0); }  /* no credential → not authenticated */
    struct picomesh_int64_result storedr = kv_get_int(&h, hdrs, k, 0);
    close_storage(&h);
    if (PICOMESH_IS_ERR(storedr)) return PICOMESH_ERR(picomesh_int, "password_authn_authenticate: read failed", storedr);
    return PICOMESH_OK(picomesh_int, storedr.value == hash ? 1 : 0);
}

PICOMESH_CLASS_ANNOTATE("override@password_authn:password_authn:password_authn_change_password")
struct picomesh_int_result password_authn_password_authn_change_password_impl(struct ctx *ctx,
                                                                  struct object *obj,
                                                                  struct yheaders *hdrs,
                                                                  uint32_t user_id, int64_t hash)
{
    (void)ctx; (void)obj;
    struct pw_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "password_authn_change_password: open_storage failed", sr);
    struct pw_storage_handle h = sr.value;
    char k[64];
    snprintf(k, sizeof(k), "hash:%u", user_id);
    struct picomesh_int_result ex = kv_exists(&h, hdrs, k);
    if (PICOMESH_IS_ERR(ex)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "password_authn_change_password: existence check failed", ex); }
    if (!ex.value) { close_storage(&h); return PICOMESH_OK(picomesh_int, 0); }
    struct picomesh_void_result w = kv_set_int(&h, hdrs, k, hash);
    if (PICOMESH_IS_ERR(w)) { close_storage(&h); return PICOMESH_ERR(picomesh_int, "password_authn_change_password: write failed", w); }
    close_storage(&h);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@password_authn:password_authn:password_authn_count_registered")
struct picomesh_size_result password_authn_password_authn_count_registered_impl(struct ctx *ctx,
                                                                    struct object *obj,
                                                                    struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct pw_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "password_authn_count: open_storage failed", sr);
    struct pw_storage_handle h = sr.value;
    struct picomesh_int64_result cr = kv_get_int(&h, hdrs, "count", 0);
    close_storage(&h);
    if (PICOMESH_IS_ERR(cr)) return PICOMESH_ERR(picomesh_size, "password_authn_count: read failed", cr);
    return PICOMESH_OK(picomesh_size, (size_t)(cr.value < 0 ? 0 : cr.value));
}

/* List ALL password_authn entries as a JSON array (gh#15) — every object,
 * not a count. NOTE: the values are credential material (hashes); this is an
 * internal/admin-console surface only. Delegates to the namespace scan. */
PICOMESH_CLASS_ANNOTATE("override@password_authn:password_authn:password_authn_list")
struct picomesh_json_result password_authn_password_authn_list_impl(struct ctx *ctx, struct object *obj,
                                                           struct yheaders *hdrs,
                                                           int64_t offset, int64_t limit)
{
    (void)ctx; (void)obj;
    struct pw_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "password_authn_list: storage open failed", sr);
    struct pw_storage_handle h = sr.value;
    return sharded_storage_db_list(&h.c, h.obj, hdrs, PW_CTX, "hash:", offset, limit);
}

/* Unbounded variant — every credential entry. Internal/admin only; care. */
PICOMESH_CLASS_ANNOTATE("override@password_authn:password_authn:password_authn_list_all")
struct picomesh_json_result password_authn_password_authn_list_all_impl(struct ctx *ctx, struct object *obj,
                                                                        struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct pw_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "password_authn_list_all: storage open failed", sr);
    struct pw_storage_handle h = sr.value;
    return sharded_storage_db_list_all(&h.c, h.obj, hdrs, PW_CTX, "hash:");
}

#include "store.gen.c"
