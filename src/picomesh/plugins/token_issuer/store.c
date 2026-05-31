/* token_issuer — JWT mint + revocation.
 *
 * In yaapp this signs RS256 JWTs against an in-memory keypair. We
 * skip the crypto here — tokens are opaque uint32 ids backed by a
 * server-side table, exactly the surface the gateway needs to call:
 *
 *   login(user_id, provider_id)   → token_id (0 on failure)
 *   validate(token_id)            → user_id (0 if absent / revoked)
 *   refresh(token_id)             → new_token_id (rotates)
 *   revoke(token_id)              → 1 ok / 0 unknown
 *   count_active                  → number of live tokens
 *
 * Refresh-token + access-token bifurcation is a presentation detail
 * — for the framework demonstration the single-token model is enough. */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>

#include <stdint.h>

#define TI_MAX 512

struct ti_entry {
    uint32_t token_id;
    uint32_t user_id;
    uint32_t provider_id;
    int used;
};

struct PICOMESH_CLASS_ANNOTATE("class@token_issuer:store") token_issuer_store_data {
    struct ti_entry entries[TI_MAX];
    size_t count;
    uint32_t next_id;
};

static struct token_issuer_store_data *ti(struct object *obj)
{
    return (struct token_issuer_store_data *)((char *)obj + sizeof(struct object));
}

static uint32_t ti_alloc(struct token_issuer_store_data *d, uint32_t user_id,
                         uint32_t provider_id)
{
    if (d->next_id == 0) d->next_id = 1;
    for (size_t i = 0; i < TI_MAX; ++i) {
        if (!d->entries[i].used) {
            d->entries[i].token_id = d->next_id++;
            d->entries[i].user_id = user_id;
            d->entries[i].provider_id = provider_id;
            d->entries[i].used = 1;
            d->count++;
            return d->entries[i].token_id;
        }
    }
    return 0;
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:store:store_login")
struct picomesh_uint32_result token_issuer_store_login_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t user_id, uint32_t provider_id)
{
    (void)ctx;
    uint32_t tid = ti_alloc(ti(obj), user_id, provider_id);
    if (!tid) return PICOMESH_ERR(picomesh_uint32, "token_issuer_login: out of slots");
    yinfo("token_issuer: minted tid=%u user=%u provider=%u", tid, user_id, provider_id);
    return PICOMESH_OK(picomesh_uint32, tid);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:store:store_validate")
struct picomesh_uint32_result token_issuer_store_validate_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                            uint32_t token_id)
{
    (void)ctx;
    struct token_issuer_store_data *d = ti(obj);
    for (size_t i = 0; i < TI_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].token_id == token_id) {
            return PICOMESH_OK(picomesh_uint32, d->entries[i].user_id);
        }
    }
    return PICOMESH_OK(picomesh_uint32, 0);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:store:store_refresh")
struct picomesh_uint32_result token_issuer_store_refresh_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                           uint32_t token_id)
{
    (void)ctx;
    struct token_issuer_store_data *d = ti(obj);
    for (size_t i = 0; i < TI_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].token_id == token_id) {
            uint32_t user_id = d->entries[i].user_id;
            uint32_t provider_id = d->entries[i].provider_id;
            d->entries[i].used = 0;
            d->count--;
            uint32_t tid = ti_alloc(d, user_id, provider_id);
            if (!tid) return PICOMESH_ERR(picomesh_uint32, "token_issuer_refresh: alloc failed");
            return PICOMESH_OK(picomesh_uint32, tid);
        }
    }
    return PICOMESH_OK(picomesh_uint32, 0);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:store:store_revoke")
struct picomesh_int_result token_issuer_store_revoke_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                       uint32_t token_id)
{
    (void)ctx;
    struct token_issuer_store_data *d = ti(obj);
    for (size_t i = 0; i < TI_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].token_id == token_id) {
            d->entries[i].used = 0;
            d->count--;
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_OK(picomesh_int, 0);
}

PICOMESH_CLASS_ANNOTATE("override@token_issuer:store:store_count_active")
struct picomesh_size_result token_issuer_store_count_active_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return PICOMESH_OK(picomesh_size, ti(obj)->count);
}

#include "store.gen.c"
