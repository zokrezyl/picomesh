/* storage_sql — SQLite-backed string→int64 store.
 *
 * Mirrors yaapp's `SQLiteStorage` in shape: a single sqlite3 file
 * holds one table per namespace. The methods exposed here use a
 * single hardcoded namespace `kv` for simplicity — multi-namespace
 * support is straightforward once we surface a namespace argument
 * through the wire (which we now can, since `const char *` is on
 * the wire too).
 *
 *   set(key, value)    → 1 (insert or replace)
 *   get(key)           → value (0 if absent — caller checks `exists`)
 *   exists(key)        → 1/0
 *   del(key)           → 1 removed / 0 unknown
 *   count              → row count
 *
 * The db_path comes from yconfig (`storage.db_path`), defaulting to
 * a per-class file. We use `:memory:` when no engine is active (CLI
 * smoke). */

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/class.h>
#include <yaafc/yengine/engine.h>
#include <yaafc/yconfig/yconfig.h>

#include <sqlite3.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct [[clang::annotate("class@storage:sql")]] storage_sql_data {
    sqlite3 *db;
    int opened;
};

static struct storage_sql_data *sd(struct object *obj)
{
    return (struct storage_sql_data *)((char *)obj + sizeof(struct object));
}

static const char *resolve_db_path(void)
{
    struct yaafc_engine *e = yaafc_active_engine();
    if (!e) return ":memory:";
    struct yconfig_node_ptr_result r =
        yconfig_get(yaafc_engine_config(e), "storage.db_path");
    if (YAAFC_IS_OK(r) && r.value) {
        const char *s = yconfig_node_as_string(r.value, NULL);
        if (s && *s) return s;
    }
    return ":memory:";
}

static int ensure_open(struct storage_sql_data *d)
{
    if (d->opened) return SQLITE_OK;
    const char *path = resolve_db_path();
    ydebug("storage_sql: opening %s", path);
    int rc = sqlite3_open(path, &d->db);
    if (rc != SQLITE_OK) {
        ywarn("storage_sql: sqlite3_open failed: %s", sqlite3_errmsg(d->db));
        return rc;
    }
    const char *ddl =
        "CREATE TABLE IF NOT EXISTS kv ("
        "  k TEXT PRIMARY KEY,"
        "  v INTEGER NOT NULL"
        ");";
    char *err = NULL;
    rc = sqlite3_exec(d->db, ddl, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        ywarn("storage_sql: DDL failed: %s", err ? err : "(no msg)");
        sqlite3_free(err);
        return rc;
    }
    d->opened = 1;
    return SQLITE_OK;
}

[[clang::annotate("override@storage:sql:sql_set")]]
struct yaafc_int_result storage_sql_set_impl(struct ctx *ctx, struct object *obj,
                                             const char *key, int64_t value)
{
    (void)ctx;
    struct storage_sql_data *d = sd(obj);
    if (ensure_open(d) != SQLITE_OK) {
        return YAAFC_ERR(yaafc_int, "sql_set: sqlite open failed");
    }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO kv(k, v) VALUES(?, ?);";
    int rc = sqlite3_prepare_v2(d->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return YAAFC_ERR(yaafc_int, "sql_set: prepare failed");
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, value);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return YAAFC_ERR(yaafc_int, "sql_set: step failed");
    yinfo("sql_set: %s=%lld", key, (long long)value);
    return YAAFC_OK(yaafc_int, 1);
}

[[clang::annotate("override@storage:sql:sql_get")]]
struct yaafc_int64_result storage_sql_get_impl(struct ctx *ctx, struct object *obj,
                                               const char *key)
{
    (void)ctx;
    struct storage_sql_data *d = sd(obj);
    if (ensure_open(d) != SQLITE_OK) {
        return YAAFC_ERR(yaafc_int64, "sql_get: sqlite open failed");
    }
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(d->db, "SELECT v FROM kv WHERE k=?;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return YAAFC_ERR(yaafc_int64, "sql_get: prepare failed");
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int64_t v = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        return YAAFC_OK(yaafc_int64, v);
    }
    sqlite3_finalize(stmt);
    return YAAFC_ERR(yaafc_int64, "sql_get: key not found");
}

[[clang::annotate("override@storage:sql:sql_exists")]]
struct yaafc_int_result storage_sql_exists_impl(struct ctx *ctx, struct object *obj,
                                                const char *key)
{
    (void)ctx;
    struct storage_sql_data *d = sd(obj);
    if (ensure_open(d) != SQLITE_OK) {
        return YAAFC_ERR(yaafc_int, "sql_exists: sqlite open failed");
    }
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(d->db, "SELECT 1 FROM kv WHERE k=?;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return YAAFC_ERR(yaafc_int, "sql_exists: prepare failed");
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int present = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    return YAAFC_OK(yaafc_int, present);
}

[[clang::annotate("override@storage:sql:sql_del")]]
struct yaafc_int_result storage_sql_del_impl(struct ctx *ctx, struct object *obj,
                                             const char *key)
{
    (void)ctx;
    struct storage_sql_data *d = sd(obj);
    if (ensure_open(d) != SQLITE_OK) {
        return YAAFC_ERR(yaafc_int, "sql_del: sqlite open failed");
    }
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(d->db, "DELETE FROM kv WHERE k=?;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return YAAFC_ERR(yaafc_int, "sql_del: prepare failed");
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return YAAFC_ERR(yaafc_int, "sql_del: step failed");
    return YAAFC_OK(yaafc_int, sqlite3_changes(d->db) > 0 ? 1 : 0);
}

[[clang::annotate("override@storage:sql:sql_count")]]
struct yaafc_size_result storage_sql_count_impl(struct ctx *ctx, struct object *obj)
{
    (void)ctx;
    struct storage_sql_data *d = sd(obj);
    if (ensure_open(d) != SQLITE_OK) {
        return YAAFC_ERR(yaafc_size, "sql_count: sqlite open failed");
    }
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(d->db, "SELECT COUNT(*) FROM kv;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return YAAFC_ERR(yaafc_size, "sql_count: prepare failed");
    rc = sqlite3_step(stmt);
    int64_t n = (rc == SQLITE_ROW) ? sqlite3_column_int64(stmt, 0) : 0;
    sqlite3_finalize(stmt);
    return YAAFC_OK(yaafc_size, (size_t)n);
}

#include "sql.gen.c"
