/* session — session-id ↔ user/token mapping.
 *
 * Scenario shape:
 *   start(user_id, provider_id)  → uint32 sid (mints, stores)
 *   lookup(sid)                  → uint32 user_id (0 if absent / expired)
 *   destroy(sid)                 → 1 if removed, 0 if unknown
 *   count_active                 → number of sessions live now
 *
 * In yaapp this composes `token_issuer.token_issuer.login` to wrap
 * the JWT inside the mesh. We keep just the storage shape — the
 * cross-plugin call would dispatch via a held remote session (added
 * later). */

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/class.h>

#include <stdint.h>

#define SESSION_MAX 256

struct session_entry {
    uint32_t sid;
    uint32_t user_id;
    uint32_t provider_id;
    int used;
};

struct [[clang::annotate("class@session:store")]] session_store_data {
    struct session_entry entries[SESSION_MAX];
    size_t count;
    uint32_t next_sid;
};

static struct session_store_data *ss(struct object *obj)
{
    return (struct session_store_data *)((char *)obj + sizeof(struct object));
}

[[clang::annotate("override@session:store:store_start")]]
struct yaafc_uint32_result session_store_start_impl(struct ctx *ctx, struct object *obj,
                                                    uint32_t user_id, uint32_t provider_id)
{
    (void)ctx;
    struct session_store_data *d = ss(obj);
    if (d->next_sid == 0) d->next_sid = 1;
    for (size_t i = 0; i < SESSION_MAX; ++i) {
        if (!d->entries[i].used) {
            d->entries[i].sid = d->next_sid++;
            d->entries[i].user_id = user_id;
            d->entries[i].provider_id = provider_id;
            d->entries[i].used = 1;
            d->count++;
            yinfo("session: sid=%u user=%u provider=%u", d->entries[i].sid,
                  user_id, provider_id);
            return YAAFC_OK(yaafc_uint32, d->entries[i].sid);
        }
    }
    return YAAFC_ERR(yaafc_uint32, "session_start: table full");
}

[[clang::annotate("override@session:store:store_lookup")]]
struct yaafc_uint32_result session_store_lookup_impl(struct ctx *ctx, struct object *obj,
                                                     uint32_t sid)
{
    (void)ctx;
    struct session_store_data *d = ss(obj);
    for (size_t i = 0; i < SESSION_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].sid == sid) {
            return YAAFC_OK(yaafc_uint32, d->entries[i].user_id);
        }
    }
    return YAAFC_OK(yaafc_uint32, 0);
}

[[clang::annotate("override@session:store:store_destroy")]]
struct yaafc_int_result session_store_destroy_impl(struct ctx *ctx, struct object *obj,
                                                   uint32_t sid)
{
    (void)ctx;
    struct session_store_data *d = ss(obj);
    for (size_t i = 0; i < SESSION_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].sid == sid) {
            d->entries[i].used = 0;
            d->count--;
            return YAAFC_OK(yaafc_int, 1);
        }
    }
    return YAAFC_OK(yaafc_int, 0);
}

[[clang::annotate("override@session:store:store_count_active")]]
struct yaafc_size_result session_store_count_active_impl(struct ctx *ctx, struct object *obj)
{
    (void)ctx;
    return YAAFC_OK(yaafc_size, ss(obj)->count);
}

#include "session.gen.c"
