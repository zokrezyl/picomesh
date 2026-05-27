/* accounts plugin — in-memory user store.
 *
 * Methods (all keyed by uint32_t user id — the wire format doesn't
 * yet carry variable-length strings):
 *
 *   accounts_register(uid)           create a user; returns 1 if new, 0 if existed
 *   accounts_exists(uid)             1 if user is registered, 0 otherwise
 *   accounts_set_balance(uid, n)     set balance to n
 *   accounts_balance(uid)            current balance (int64); error if uid unknown
 *   accounts_count()                 total registered users
 *
 * Mirrors yaapp.plugins.accounts in role: a registry-of-things you
 * can interrogate and mutate over RPC. */

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/class.h>

#include <stdint.h>
#include <string.h>

#define ACC_MAX 128

struct acc_entry {
    uint32_t uid;
    int64_t balance;
    int used;
};

struct [[clang::annotate("class@accounts:store")]] accounts_store_data {
    struct acc_entry entries[ACC_MAX];
    size_t count;
};

static struct accounts_store_data *acc_data(struct object *obj)
{
    return (struct accounts_store_data *)((char *)obj + sizeof(struct object));
}

static struct acc_entry *acc_find(struct accounts_store_data *d, uint32_t uid)
{
    for (size_t i = 0; i < ACC_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].uid == uid) return &d->entries[i];
    }
    return NULL;
}

[[clang::annotate("override@accounts:store:store_register")]]
struct yaafc_int_result accounts_store_register_impl(struct ctx *ctx, struct object *obj,
                                                    uint32_t uid)
{
    (void)ctx;
    struct accounts_store_data *d = acc_data(obj);
    if (acc_find(d, uid)) {
        ydebug("accounts_register: uid=%u already exists", uid);
        return YAAFC_OK(yaafc_int, 0);
    }
    for (size_t i = 0; i < ACC_MAX; ++i) {
        if (!d->entries[i].used) {
            d->entries[i].uid = uid;
            d->entries[i].balance = 0;
            d->entries[i].used = 1;
            d->count++;
            yinfo("accounts_register: uid=%u (total=%zu)", uid, d->count);
            return YAAFC_OK(yaafc_int, 1);
        }
    }
    return YAAFC_ERR(yaafc_int, "accounts_register: store full");
}

[[clang::annotate("override@accounts:store:store_exists")]]
struct yaafc_int_result accounts_store_exists_impl(struct ctx *ctx, struct object *obj,
                                                  uint32_t uid)
{
    (void)ctx;
    return YAAFC_OK(yaafc_int, acc_find(acc_data(obj), uid) ? 1 : 0);
}

[[clang::annotate("override@accounts:store:store_set_balance")]]
struct yaafc_int_result accounts_store_set_balance_impl(struct ctx *ctx, struct object *obj,
                                                        uint32_t uid, int64_t n)
{
    (void)ctx;
    struct acc_entry *e = acc_find(acc_data(obj), uid);
    if (!e) return YAAFC_ERR(yaafc_int, "accounts_set_balance: unknown uid");
    e->balance = n;
    ydebug("accounts_set_balance: uid=%u balance=%lld", uid, (long long)n);
    return YAAFC_OK(yaafc_int, 1);
}

[[clang::annotate("override@accounts:store:store_balance")]]
struct yaafc_int64_result accounts_store_balance_impl(struct ctx *ctx, struct object *obj,
                                                      uint32_t uid)
{
    (void)ctx;
    struct acc_entry *e = acc_find(acc_data(obj), uid);
    if (!e) return YAAFC_ERR(yaafc_int64, "accounts_balance: unknown uid");
    return YAAFC_OK(yaafc_int64, e->balance);
}

[[clang::annotate("override@accounts:store:store_count")]]
struct yaafc_size_result accounts_store_count_impl(struct ctx *ctx, struct object *obj)
{
    (void)ctx;
    return YAAFC_OK(yaafc_size, acc_data(obj)->count);
}

#include "accounts.gen.c"
