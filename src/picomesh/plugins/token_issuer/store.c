/* token_issuer — opaque token mint + revocation.
 *
 *   login(user_id, provider_id)   → token_id (0 on failure)
 *   validate(token_id)            → user_id (0 if absent / revoked)
 *   refresh(token_id)             → new_token_id (rotates)
 *   revoke(token_id)              → 1 ok / 0 unknown
 *   count_active                  → number of live tokens
 *
 * State lives in the shared `sharded_storage` service (context
 * `token_issuer`), NOT in this process — so there is no fixed in-memory slot
 * table to overflow (the old TI_MAX=512 cap returned "out of slots" under
 * load), and multiple objects / gateway workers share one source of truth.
 *
 * Storage layout in the `token_issuer` context:
 *   next_id        → monotonic token-id counter
 *   count          → live token count
 *   tok:<id>       → "<user_id>\t<provider_id>"
 *
 * (yaapp signs RS256 JWTs against an in-memory keypair; we keep the opaque
 * server-side-table surface the gateway needs — the crypto is out of scope
 * for the framework demonstration.) */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/plugin/sharded_storage/sharded_storage.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TI_CTX "token_issuer"

/* No in-memory state — every op delegates to storage. */
struct PICOMESH_CLASS_ANNOTATE("class@token_issuer:token_issuer") token_issuer_token_issuer_data {
    char _unused;
};

struct ti_storage {
    struct ctx c;
    struct object *obj;
};
PICOMESH_RESULT_DECLARE(ti_storage, struct ti_storage);

static struct ti_storage_result ti_open(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(ti_storage, "token_issuer: no active engine");
    struct ti_storage h = {.c = picomesh_engine_service_ctx(e, "sharded_storage")};
    struct object_ptr_result o = sharded_storage_db_create(&h.c);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(ti_storage, "token_issuer: storage_db_create failed", o);
    h.obj = o.value;
    return PICOMESH_OK(ti_storage, h);
}

/* Atomic counter / id bump — OK value is the value after the add; a backend
 * failure is propagated, never collapsed into a (valid) token id 0 / 0 count. */
static struct picomesh_int64_result ti_incr(struct ti_storage *h, struct yheaders *hdrs, const char *key, int64_t delta)
{
    struct picomesh_int64_result r = sharded_storage_db_incr(&h->c, h->obj, hdrs, TI_CTX, key, delta);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "token_issuer: counter update failed", r);
    return r;
}

/* Read an int. A real backend error is propagated; an absent/empty key
 * (db_get → "") yields `fallback`. */
static struct picomesh_int64_result ti_get(struct ti_storage *h, struct yheaders *hdrs, const char *key, int64_t fallback)
{
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, TI_CTX, key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int64, "token_issuer: storage read failed", r);
    int64_t v = (r.value && r.value[0]) ? strtoll(r.value, NULL, 10) : fallback;
    free(r.value);
    return PICOMESH_OK(picomesh_int64, v);
}

/* Load tok:<id> → user_id/provider_id. OK value: 1 = present (out params
 * filled), 0 = absent. A backend read failure is propagated. */
static struct picomesh_int_result token_load(struct ti_storage *h, struct yheaders *hdrs, uint32_t token_id,
                                             uint32_t *user_id, uint32_t *provider_id)
{
    char k[40];
    snprintf(k, sizeof(k), "tok:%u", token_id);
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, TI_CTX, k);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_int, "token_issuer: token read failed", r);
    if (!r.value || !r.value[0]) { free(r.value); return PICOMESH_OK(picomesh_int, 0); }
    char *s = r.value, *t1 = strchr(s, '\t');
    if (!t1) { free(s); return PICOMESH_OK(picomesh_int, 0); }
    *t1 = 0;
    if (user_id) *user_id = (uint32_t)strtoul(s, NULL, 10);
    if (provider_id) *provider_id = (uint32_t)strtoul(t1 + 1, NULL, 10);
    free(s);
    return PICOMESH_OK(picomesh_int, 1);
}

/* Write the canonical token row; propagates a failed write. */
static struct picomesh_void_result token_store(struct ti_storage *h, struct yheaders *hdrs, uint32_t token_id,
                                               uint32_t user_id, uint32_t provider_id)
{
    char k[40], v[40];
    snprintf(k, sizeof(k), "tok:%u", token_id);
    snprintf(v, sizeof(v), "%u\t%u", user_id, provider_id);
    struct picomesh_int_result r = sharded_storage_db_set(&h->c, h->obj, hdrs, TI_CTX, k, v);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "token_issuer: token write failed", r);
    return PICOMESH_OK_VOID();
}

/* Mint a fresh token row for (user, provider) and return its id: atomic id
 * via db_incr + a row write (both propagate). Does NOT touch the live count —
 * callers that change the population (login) bump it; rotation (refresh)
 * leaves it unchanged. */
static struct picomesh_uint32_result token_mint(struct ti_storage *h, struct yheaders *hdrs,
                                                uint32_t user_id, uint32_t provider_id)
{
    struct picomesh_int64_result idr = ti_incr(h, hdrs, "next_id", 1);
    if (PICOMESH_IS_ERR(idr)) return PICOMESH_ERR(picomesh_uint32, "token_issuer: allocate id failed", idr);
    uint32_t tid = (uint32_t)idr.value;
    struct picomesh_void_result ws = token_store(h, hdrs, tid, user_id, provider_id);
    if (PICOMESH_IS_ERR(ws)) return PICOMESH_ERR(picomesh_uint32, "token_issuer: persist token failed", ws);
    return PICOMESH_OK(picomesh_uint32, tid);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_login")
struct picomesh_uint32_result token_issuer_token_issuer_login_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t user_id, uint32_t provider_id)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_login: storage open failed", sr);
    struct ti_storage h = sr.value;
    struct picomesh_uint32_result minted = token_mint(&h, hdrs, user_id, provider_id);
    if (PICOMESH_IS_ERR(minted)) return minted;  /* chain already set by token_mint */
    struct picomesh_int64_result cc = ti_incr(&h, hdrs, "count", 1);
    if (PICOMESH_IS_ERR(cc)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_login: bump count failed", cc);
    yinfo("token_issuer: minted tid=%u user=%u provider=%u", minted.value, user_id, provider_id);
    return minted;
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_validate")
struct picomesh_uint32_result token_issuer_token_issuer_validate_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                            uint32_t token_id)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_validate: storage open failed", sr);
    struct ti_storage h = sr.value;
    uint32_t user_id = 0;
    struct picomesh_int_result lr = token_load(&h, hdrs, token_id, &user_id, NULL);
    if (PICOMESH_IS_ERR(lr)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_validate: load failed", lr);
    return PICOMESH_OK(picomesh_uint32, lr.value ? user_id : 0);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_refresh")
struct picomesh_uint32_result token_issuer_token_issuer_refresh_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                           uint32_t token_id)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_refresh: storage open failed", sr);
    struct ti_storage h = sr.value;
    uint32_t user_id = 0, provider_id = 0;
    struct picomesh_int_result lr = token_load(&h, hdrs, token_id, &user_id, &provider_id);
    if (PICOMESH_IS_ERR(lr)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_refresh: load failed", lr);
    if (lr.value == 0) return PICOMESH_OK(picomesh_uint32, 0);  /* unknown token */
    /* Rotate atomically: db_del the old row (gated on actually removing it),
     * then mint a new one. Population is unchanged (one out, one in), so the
     * count is NOT adjusted. If the del loses the race (a concurrent
     * refresh/revoke removed it first), return 0. */
    char k[40];
    snprintf(k, sizeof(k), "tok:%u", token_id);
    struct picomesh_int_result del = sharded_storage_db_del(&h.c, h.obj, hdrs, TI_CTX, k);
    if (PICOMESH_IS_ERR(del)) return PICOMESH_ERR(picomesh_uint32, "token_issuer_refresh: delete failed", del);
    if (del.value == 0) return PICOMESH_OK(picomesh_uint32, 0);  /* already rotated/revoked */
    struct picomesh_uint32_result minted = token_mint(&h, hdrs, user_id, provider_id);
    if (PICOMESH_IS_ERR(minted)) return minted;
    return minted;
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_revoke")
struct picomesh_int_result token_issuer_token_issuer_revoke_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                       uint32_t token_id)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "token_issuer_revoke: storage open failed", sr);
    struct ti_storage h = sr.value;
    /* db_del is the atomic point: exactly one of two concurrent revokes sees
     * removed==1 and decrements the count. */
    char k[40];
    snprintf(k, sizeof(k), "tok:%u", token_id);
    struct picomesh_int_result del = sharded_storage_db_del(&h.c, h.obj, hdrs, TI_CTX, k);
    if (PICOMESH_IS_ERR(del)) return PICOMESH_ERR(picomesh_int, "token_issuer_revoke: delete failed", del);
    if (del.value == 0) return PICOMESH_OK(picomesh_int, 0);  /* unknown / already gone */
    struct picomesh_int64_result cc = ti_incr(&h, hdrs, "count", -1);
    if (PICOMESH_IS_ERR(cc)) return PICOMESH_ERR(picomesh_int, "token_issuer_revoke: count update failed", cc);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_count_active")
struct picomesh_size_result token_issuer_token_issuer_count_active_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "token_issuer_count: storage open failed", sr);
    struct ti_storage h = sr.value;
    struct picomesh_int64_result cr = ti_get(&h, hdrs, "count", 0);
    if (PICOMESH_IS_ERR(cr)) return PICOMESH_ERR(picomesh_size, "token_issuer_count: read failed", cr);
    return PICOMESH_OK(picomesh_size, (size_t)(cr.value < 0 ? 0 : cr.value));
}

/* List ALL issued-token entries as a JSON array (gh#15) — every object,
 * not a count. Delegates to the namespace scan. */
PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_list")
struct picomesh_json_result token_issuer_token_issuer_list_impl(struct ctx *ctx, struct object *obj,
                                                         struct yheaders *hdrs,
                                                         int64_t offset, int64_t limit)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "token_issuer_list: storage open failed", sr);
    struct ti_storage h = sr.value;
    return sharded_storage_db_list(&h.c, h.obj, hdrs, TI_CTX, "tok:", offset, limit);
}

/* Unbounded variant — every issued token. Use with care on large deployments. */
PICOMESH_CLASS_ANNOTATE("override@token_issuer:token_issuer:token_issuer_list_all")
struct picomesh_json_result token_issuer_token_issuer_list_all_impl(struct ctx *ctx, struct object *obj,
                                                                    struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct ti_storage_result sr = ti_open();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_json, "token_issuer_list_all: storage open failed", sr);
    struct ti_storage h = sr.value;
    return sharded_storage_db_list_all(&h.c, h.obj, hdrs, TI_CTX, "tok:");
}

#include "store.gen.c"
