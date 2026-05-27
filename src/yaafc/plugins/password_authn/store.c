/* password_authn — password verification.
 *
 *   register(user_id, pw_hash)                → 1 created, 0 already-exists
 *   authenticate(user_id, pw_hash)            → 1 ok, 0 mismatch / unknown
 *   change_password(user_id, new_hash)        → 1 ok, 0 unknown
 *   count_registered                          → size
 *
 * Passwords are compared by hash: the caller hashes (PBKDF2 / argon2
 * etc.), wire-transports the result as an int64 (truncated to fit
 * the current scalar-only wire format), and we store + compare bit-
 * for-bit. Real password infra would carry the full digest as bytes
 * once the wire grows string support. */

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/class.h>

#include <stdint.h>

#define PW_MAX 256

struct pw_entry {
    uint32_t user_id;
    int64_t  hash;
    int used;
};

struct [[clang::annotate("class@password_authn:store")]] password_authn_store_data {
    struct pw_entry entries[PW_MAX];
    size_t count;
};

static struct password_authn_store_data *pw(struct object *obj)
{
    return (struct password_authn_store_data *)((char *)obj + sizeof(struct object));
}

static struct pw_entry *pw_find(struct password_authn_store_data *d, uint32_t uid)
{
    for (size_t i = 0; i < PW_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].user_id == uid) return &d->entries[i];
    }
    return NULL;
}

[[clang::annotate("override@password_authn:store:store_register")]]
struct yaafc_int_result password_authn_store_register_impl(struct ctx *ctx, struct object *obj,
                                                           uint32_t user_id, int64_t hash)
{
    (void)ctx;
    struct password_authn_store_data *d = pw(obj);
    if (pw_find(d, user_id)) return YAAFC_OK(yaafc_int, 0);
    for (size_t i = 0; i < PW_MAX; ++i) {
        if (!d->entries[i].used) {
            d->entries[i].user_id = user_id;
            d->entries[i].hash = hash;
            d->entries[i].used = 1;
            d->count++;
            yinfo("password_authn: registered uid=%u", user_id);
            return YAAFC_OK(yaafc_int, 1);
        }
    }
    return YAAFC_ERR(yaafc_int, "password_authn_register: table full");
}

[[clang::annotate("override@password_authn:store:store_authenticate")]]
struct yaafc_int_result password_authn_store_authenticate_impl(struct ctx *ctx,
                                                               struct object *obj,
                                                               uint32_t user_id, int64_t hash)
{
    (void)ctx;
    struct pw_entry *e = pw_find(pw(obj), user_id);
    if (!e) return YAAFC_OK(yaafc_int, 0);
    return YAAFC_OK(yaafc_int, e->hash == hash ? 1 : 0);
}

[[clang::annotate("override@password_authn:store:store_change_password")]]
struct yaafc_int_result password_authn_store_change_password_impl(struct ctx *ctx,
                                                                  struct object *obj,
                                                                  uint32_t user_id, int64_t hash)
{
    (void)ctx;
    struct pw_entry *e = pw_find(pw(obj), user_id);
    if (!e) return YAAFC_OK(yaafc_int, 0);
    e->hash = hash;
    return YAAFC_OK(yaafc_int, 1);
}

[[clang::annotate("override@password_authn:store:store_count_registered")]]
struct yaafc_size_result password_authn_store_count_registered_impl(struct ctx *ctx,
                                                                    struct object *obj)
{
    (void)ctx;
    return YAAFC_OK(yaafc_size, pw(obj)->count);
}

#include "store.gen.c"
