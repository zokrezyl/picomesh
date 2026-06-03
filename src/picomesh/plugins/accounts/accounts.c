/* accounts — the user registry, as RELATIONAL ROWS.
 *
 * A user is a ROW in the `users` table (real columns), stored in the
 * `relational_storage` service — not a scatter of `user:<uid>`, `name:<uid>`,
 * `groups:<uid>`, `balance:<uid>` keys in a KV store. The accounts plugin OWNS
 * this table: it runs `CREATE TABLE IF NOT EXISTS` itself, once per worker; the
 * consumer app does not define another plugin's schema.
 *
 *   users(uid PK, username UNIQUE, groups, balance, created_at)
 *
 *   register(uid, username)  1 newly created / 0 already exists
 *   exists(uid)              1 / 0
 *   set_balance(uid, n)      1 (error if uid unknown)
 *   balance(uid)             current balance (error if uid unknown)
 *   set_groups(uid, csv)     store "<account>:<role>,…" memberships
 *   groups(uid)              the groups CSV ("" if none)
 *   count()                  live user count
 *   list / list_all          [{"uid":…,"username":…}, …]
 */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/yplatform/time.h>
#include <picomesh/plugin/relational_storage/relational_sql.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACCOUNTS_DDL \
    "CREATE TABLE IF NOT EXISTS users(" \
    "uid INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, " \
    "groups TEXT NOT NULL DEFAULT '', balance INTEGER NOT NULL DEFAULT 0, " \
    "created_at INTEGER NOT NULL DEFAULT 0)"

struct PICOMESH_CLASS_ANNOTATE("class@accounts:accounts") accounts_accounts_data {
    int schema_ensured; /* per-worker: set once this worker has created the table */
};

static struct accounts_accounts_data *acc(struct object *obj)
{
    return (struct accounts_accounts_data *)((char *)obj + sizeof(struct object));
}

static struct picomesh_void_result accounts_open(struct rel_handle *h, struct yheaders *hdrs, struct object *obj)
{
    struct picomesh_void_result o = rel_open(h);
    if (PICOMESH_IS_ERR(o)) return PICOMESH_ERR(picomesh_void, "accounts: open relational_storage failed", o);
    return rel_ensure_schema(h, hdrs, &acc(obj)->schema_ensured, ACCOUNTS_DDL);
}

static int account_exists(struct rel_handle *h, struct yheaders *hdrs, uint32_t uid)
{
    char *args = rel_args1i((int64_t)uid);
    int found = 0;
    rel_query_int(h, hdrs, "SELECT uid FROM users WHERE uid=?", args, "uid", 0, &found);
    free(args);
    return found;
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_register")
struct picomesh_int_result accounts_accounts_register_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                    uint32_t uid, const char *username)
{
    (void)ctx;
    if (!username) username = "";
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_int, "accounts_register: open failed", oh);

    /* INSERT OR IGNORE atomically elects one registrant: a PK (uid) or UNIQUE
     * (username) conflict is ignored, so changes==1 means newly created. */
    struct yjson_writer *aw = yjson_writer_new();
    yjson_writer_begin_array(aw);
    yjson_writer_int(aw, (int64_t)uid);
    yjson_writer_string(aw, username);
    yjson_writer_int(aw, picomesh_yplatform_time_wall_ms() / 1000);
    char *args = rel_args_take(aw);
    int changes = rel_exec_changes(&h, hdrs,
        "INSERT OR IGNORE INTO users(uid,username,created_at) VALUES(?,?,?)", args);
    free(args);
    if (changes < 0) return PICOMESH_ERR(picomesh_int, "accounts_register: insert failed");
    if (changes == 0) { ydebug("accounts_register: uid=%u already exists", uid); return PICOMESH_OK(picomesh_int, 0); }
    yinfo("accounts_register: uid=%u name=%s", uid, username);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_exists")
struct picomesh_int_result accounts_accounts_exists_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                  uint32_t uid)
{
    (void)ctx;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_int, "accounts_exists: open failed", oh);
    return PICOMESH_OK(picomesh_int, account_exists(&h, hdrs, uid) ? 1 : 0);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_set_balance")
struct picomesh_int_result accounts_accounts_set_balance_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                        uint32_t uid, int64_t n)
{
    (void)ctx;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_int, "accounts_set_balance: open failed", oh);
    char *args = rel_args2i(n, (int64_t)uid);
    int changes = rel_exec_changes(&h, hdrs, "UPDATE users SET balance=? WHERE uid=?", args);
    free(args);
    if (changes < 0) return PICOMESH_ERR(picomesh_int, "accounts_set_balance: write failed");
    if (changes == 0) return PICOMESH_ERR(picomesh_int, "accounts_set_balance: unknown uid");
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_balance")
struct picomesh_int64_result accounts_accounts_balance_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                      uint32_t uid)
{
    (void)ctx;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_int64, "accounts_balance: open failed", oh);
    char *args = rel_args1i((int64_t)uid);
    int found = 0;
    int64_t bal = rel_query_int(&h, hdrs, "SELECT balance FROM users WHERE uid=?", args, "balance", 0, &found);
    free(args);
    if (!found) return PICOMESH_ERR(picomesh_int64, "accounts_balance: unknown uid");
    return PICOMESH_OK(picomesh_int64, bal);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_count")
struct picomesh_size_result accounts_accounts_count_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_size, "accounts_count: open failed", oh);
    int64_t n = rel_query_int(&h, hdrs, "SELECT COUNT(*) AS n FROM users", "[]", "n", 0, NULL);
    return PICOMESH_OK(picomesh_size, (size_t)(n < 0 ? 0 : n));
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_set_groups")
struct picomesh_int_result accounts_accounts_set_groups_impl(struct ctx *ctx, struct object *obj,
                                                             struct yheaders *hdrs,
                                                             uint32_t uid, const char *groups_csv)
{
    (void)ctx;
    if (!groups_csv) groups_csv = "";
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_int, "accounts_set_groups: open failed", oh);
    struct yjson_writer *aw = yjson_writer_new();
    yjson_writer_begin_array(aw);
    yjson_writer_string(aw, groups_csv);
    yjson_writer_int(aw, (int64_t)uid);
    char *args = rel_args_take(aw);
    int changes = rel_exec_changes(&h, hdrs, "UPDATE users SET groups=? WHERE uid=?", args);
    free(args);
    if (changes < 0) return PICOMESH_ERR(picomesh_int, "accounts_set_groups: write failed");
    if (changes == 0) return PICOMESH_ERR(picomesh_int, "accounts_set_groups: unknown uid");
    yinfo("accounts_set_groups: uid=%u groups=%s", uid, groups_csv);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_groups")
struct picomesh_string_result accounts_accounts_groups_impl(struct ctx *ctx, struct object *obj,
                                                            struct yheaders *hdrs, uint32_t uid)
{
    (void)ctx;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_string, "accounts_groups: open failed", oh);
    char *args = rel_args1i((int64_t)uid);
    char *groups = rel_query_str(&h, hdrs, "SELECT groups FROM users WHERE uid=?", args, "groups");
    free(args);
    if (!groups) {
        char *empty = strdup("");
        return empty ? PICOMESH_OK(picomesh_string, empty) : PICOMESH_ERR(picomesh_string, "accounts_groups: out of memory");
    }
    return PICOMESH_OK(picomesh_string, groups);
}

/* List registered users as `[{"uid":…,"username":…}, …]` — a plain SELECT. */
PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_list")
struct picomesh_json_result accounts_accounts_list_impl(struct ctx *ctx, struct object *obj,
                                                        struct yheaders *hdrs,
                                                        int64_t offset, int64_t limit)
{
    (void)ctx;
    if (limit <= 0) limit = 100;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_json, "accounts_list: open failed", oh);
    char *args = rel_args2i(limit, offset < 0 ? 0 : offset);
    struct picomesh_json_result r =
        rel_query(&h, hdrs, "SELECT uid,username FROM users ORDER BY uid LIMIT ? OFFSET ?", args);
    free(args);
    return r;
}

PICOMESH_CLASS_ANNOTATE("override@accounts:accounts:accounts_list_all")
struct picomesh_json_result accounts_accounts_list_all_impl(struct ctx *ctx, struct object *obj,
                                                            struct yheaders *hdrs)
{
    (void)ctx;
    struct rel_handle h;
    struct picomesh_void_result oh = accounts_open(&h, hdrs, obj);
    if (PICOMESH_IS_ERR(oh)) return PICOMESH_ERR(picomesh_json, "accounts_list_all: open failed", oh);
    return rel_query(&h, hdrs, "SELECT uid,username FROM users ORDER BY uid", "[]");
}

#include "accounts.gen.c"
