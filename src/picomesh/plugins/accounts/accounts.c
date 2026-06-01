/* accounts plugin — user registry backed by the storage service.
 *
 * Methods (uid is uint32 on the wire today; usernames are mapped to
 * uids client-side by the frontend, mirroring yaapp's accounts plugin
 * but without the string-key round-trip):
 *
 *   accounts_register(uid)        1 if newly created, 0 if already there
 *   accounts_exists(uid)          1 if present, 0 otherwise
 *   accounts_set_balance(uid, n)  set balance (errors if uid unknown)
 *   accounts_balance(uid)         current balance (errors if uid unknown)
 *   accounts_count()              live row count
 *
 * Storage layout in the `accounts` context:
 *
 *   user:<uid>      → 1 (registered marker)
 *   balance:<uid>   → int64 balance
 *   count           → number of registered users
 *
 * The plugin holds no in-memory state — every method delegates to the
 * storage service on the configured remote, using a single context
 * `accounts`. The storage service maps the context to either a sqlite
 * table or an mdbx DBI (transparent to this plugin).
 */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/plugin/sharded_storage/sharded_storage.h>
#include <picomesh/yclass/rpc.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PICOMESH_CLASS_ANNOTATE("class@accounts:store") accounts_store_data {
    char _unused;
};

struct acc_storage_handle {
    struct ctx c;
    struct object *obj;
};
PICOMESH_RESULT_DECLARE(acc_storage_handle, struct acc_storage_handle);

static struct acc_storage_handle_result open_storage(void)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(acc_storage_handle, "accounts: no active engine");
    struct acc_storage_handle h = {.c = picomesh_engine_service_ctx(e, "sharded_storage")};
    /* peer==NULL ⇒ storage is collocated in THIS process; create resolves it
     * as a local in-process object. A non-NULL peer is the remote-mesh path.
     * Both go through sharded_storage_db_create — don't bail on a missing peer. */
    struct object_ptr_result o = sharded_storage_db_create(&h.c);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(acc_storage_handle, "accounts: storage_db_create failed", o);
    h.obj = o.value;
    return PICOMESH_OK(acc_storage_handle, h);
}

#define ACCOUNTS_CTX "accounts"

static void close_storage(struct acc_storage_handle *h)
{
    /* The storage object is a cached, service-lifetime dependency
     * (rpc_object_acquire owns it); nothing to release per call. */
    (void)h;
}

/* The store holds string values; account state is integer counters and
 * balances, so serialize as decimal strings and parse them back. */
static void kv_set_int(struct acc_storage_handle *h, struct yheaders *hdrs,
                       const char *key, int64_t value)
{
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "%lld", (long long)value);
    sharded_storage_db_set(&h->c, h->obj, hdrs, ACCOUNTS_CTX, key, vbuf);
}

static int64_t kv_get_or(struct acc_storage_handle *h, struct yheaders *hdrs, const char *key, int64_t fallback)
{
    struct picomesh_string_result r = sharded_storage_db_get(&h->c, h->obj, hdrs, ACCOUNTS_CTX, key);
    if (PICOMESH_IS_ERR(r)) { picomesh_error_destroy(r.error); return fallback; }
    int64_t v = r.value ? strtoll(r.value, NULL, 10) : fallback;
    free(r.value);
    return v;
}

static int kv_exists(struct acc_storage_handle *h, struct yheaders *hdrs, const char *key)
{
    struct picomesh_int_result r = sharded_storage_db_exists(&h->c, h->obj, hdrs, ACCOUNTS_CTX, key);
    int present = PICOMESH_IS_OK(r) && r.value;
    if (PICOMESH_IS_ERR(r)) picomesh_error_destroy(r.error);
    return present;
}

PICOMESH_CLASS_ANNOTATE("override@accounts:store:store_register")
struct picomesh_int_result accounts_store_register_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                    uint32_t uid)
{
    (void)ctx; (void)obj;
    struct acc_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "accounts_register: open_storage failed", sr);
    struct acc_storage_handle h = sr.value;

    char k[64];
    snprintf(k, sizeof(k), "user:%u", uid);
    if (kv_exists(&h, hdrs, k)) {
        close_storage(&h);
        ydebug("accounts_register: uid=%u already exists", uid);
        return PICOMESH_OK(picomesh_int, 0);
    }
    kv_set_int(&h, hdrs, k, 1);
    int64_t count = kv_get_or(&h, hdrs, "count", 0) + 1;
    kv_set_int(&h, hdrs, "count", count);
    close_storage(&h);
    yinfo("accounts_register: uid=%u (total=%lld)", uid, (long long)count);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:store:store_exists")
struct picomesh_int_result accounts_store_exists_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                  uint32_t uid)
{
    (void)ctx; (void)obj;
    struct acc_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "accounts_exists: open_storage failed", sr);
    struct acc_storage_handle h = sr.value;
    char k[64];
    snprintf(k, sizeof(k), "user:%u", uid);
    int present = kv_exists(&h, hdrs, k);
    close_storage(&h);
    return PICOMESH_OK(picomesh_int, present);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:store:store_set_balance")
struct picomesh_int_result accounts_store_set_balance_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                        uint32_t uid, int64_t n)
{
    (void)ctx; (void)obj;
    struct acc_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int, "accounts_set_balance: open_storage failed", sr);
    struct acc_storage_handle h = sr.value;
    char k[64];
    snprintf(k, sizeof(k), "user:%u", uid);
    if (!kv_exists(&h, hdrs, k)) {
        close_storage(&h);
        return PICOMESH_ERR(picomesh_int, "accounts_set_balance: unknown uid");
    }
    snprintf(k, sizeof(k), "balance:%u", uid);
    kv_set_int(&h, hdrs, k, n);
    close_storage(&h);
    ydebug("accounts_set_balance: uid=%u balance=%lld", uid, (long long)n);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:store:store_balance")
struct picomesh_int64_result accounts_store_balance_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                      uint32_t uid)
{
    (void)ctx; (void)obj;
    struct acc_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_int64, "accounts_balance: open_storage failed", sr);
    struct acc_storage_handle h = sr.value;
    char k[64];
    snprintf(k, sizeof(k), "user:%u", uid);
    if (!kv_exists(&h, hdrs, k)) {
        close_storage(&h);
        return PICOMESH_ERR(picomesh_int64, "accounts_balance: unknown uid");
    }
    snprintf(k, sizeof(k), "balance:%u", uid);
    int64_t bal = kv_get_or(&h, hdrs, k, 0);
    close_storage(&h);
    return PICOMESH_OK(picomesh_int64, bal);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:store:store_count")
struct picomesh_size_result accounts_store_count_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct acc_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_size, "accounts_count: open_storage failed", sr);
    struct acc_storage_handle h = sr.value;
    int64_t c = kv_get_or(&h, hdrs, "count", 0);
    close_storage(&h);
    return PICOMESH_OK(picomesh_size, (size_t)(c < 0 ? 0 : c));
}

/* List the registered users as newline-separated "<uid>\t<username>" rows.
 * The frontend maps usernames → uids before calling the uid-only methods,
 * so it records the reverse mapping in the `index` key at registration
 * time (key `name:<uid>` too); this returns that index verbatim. Empty
 * string when no users have registered yet. */
PICOMESH_CLASS_ANNOTATE("override@accounts:store:store_list")
struct picomesh_string_result accounts_store_list_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx; (void)obj;
    struct acc_storage_handle_result sr = open_storage();
    if (PICOMESH_IS_ERR(sr)) return PICOMESH_ERR(picomesh_string, "accounts_list: open_storage failed", sr);
    struct acc_storage_handle h = sr.value;
    struct picomesh_string_result g =
        sharded_storage_db_get(&h.c, h.obj, hdrs, ACCOUNTS_CTX, "index");
    close_storage(&h);
    if (PICOMESH_IS_ERR(g)) { picomesh_error_destroy(g.error); return PICOMESH_OK(picomesh_string, strdup("")); }
    if (!g.value) return PICOMESH_OK(picomesh_string, strdup(""));
    return PICOMESH_OK(picomesh_string, g.value); /* transfer ownership */
}

#include "accounts.gen.c"
