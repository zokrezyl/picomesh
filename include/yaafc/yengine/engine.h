/* yaafc engine — lifecycle owner.
 *
 * Mirrors yaapp's `Yaapp` + `YaappEngine`. One process owns one
 * `struct yaafc_engine`. The engine owns:
 *
 *   - the yloop event loop (libuv).
 *   - the per-process RPC server state (object handles, skel cache).
 *
 * Plugins do NOT register with the engine at runtime — they're
 * compiled in and their `__attribute__((constructor))` hooks (emitted
 * by codegen) install the class accessor + skel lookup into the
 * global registry before main() runs. The engine just sees them via
 * `class_by_name`. */

#ifndef YAAFC_YENGINE_ENGINE_H
#define YAAFC_YENGINE_ENGINE_H

#include <yaafc/ycore/result.h>
#include <yaafc/yclass/class.h>

struct yaafc_engine;
struct yloop;
struct yconfig;
struct yconfig_node;
struct yargv_chain;

YAAFC_RESULT_DECLARE(yaafc_engine_ptr, struct yaafc_engine *);

/* Engine construction inputs. NULL → defaults. */
struct yaafc_engine_args {
    /* Pre-parsed CLI chain (owned by caller before create, owned by
     * the engine after — engine destroys it on yaafc_engine_destroy). */
    struct yargv_chain *cli;

    /* Explicit config file path (highest precedence). NULL → check
     * the CLI chain for --config-file. */
    const char *config_file;

    /* App name → drives ~/.config/<name>/<name>.yaml etc. Default
     * "yaafc". */
    const char *app_name;

    /* Skip filesystem search (XDG / git-root / cwd). */
    int no_filesystem_search;
};

/* Create + initialize. Sets up tracing, the RPC server state, the
 * yloop, and loads yconfig. `args` may be NULL — then the engine
 * loads config from defaults + the standard filesystem search.
 *
 * When `args->cli` is non-NULL, the engine reads its `--config-file`,
 * `--config K=V`, and `--env K=V` options to drive yconfig. The CLI
 * chain becomes owned by the engine. */
struct yaafc_engine_ptr_result yaafc_engine_create(const struct yaafc_engine_args *args);

void yaafc_engine_destroy(struct yaafc_engine *e);

/* Run until something stops the loop (frontend's shutdown, signal,
 * yaafc_engine_stop). */
struct yaafc_void_result yaafc_engine_run(struct yaafc_engine *e);

/* Ask the engine to wind down. Safe from any coroutine on the loop
 * thread. */
void yaafc_engine_stop(struct yaafc_engine *e);

/* Borrow the yloop for frontend code (yloop_listen_tcp etc.). The
 * engine retains ownership; do not destroy. */
struct yloop *yaafc_engine_loop(struct yaafc_engine *e);

/* Plugins reach the engine via a process-global accessor — the driver
 * calls `yaafc_active_engine_set` after `yaafc_engine_create` and the
 * plugins read it via `yaafc_active_engine`. Mirrors the role of
 * `yaapp_engine` being passed into every `__init__`. NULL is a valid
 * read (no engine yet) — plugins must handle it. */
void yaafc_active_engine_set(struct yaafc_engine *e);
struct yaafc_engine *yaafc_active_engine(void);

/* The engine's config tree. Owned by the engine; do not destroy. */
const struct yconfig *yaafc_engine_config(struct yaafc_engine *e);

/* Plugin shortcut: return the top-level `<plugin>` subtree, or NULL.
 * Mirrors `yaapp_engine.get_config('<plugin>')`. */
const struct yconfig_node *yaafc_engine_plugin_config(struct yaafc_engine *e, const char *plugin);

/* CLI chain — engine owns it, may be NULL. */
struct yargv_chain *yaafc_engine_cli(struct yaafc_engine *e);

/* ---- inter-plugin RPC clients (the "remotes:" config edge) -------- */

/* Open a TCP RPC client connection to `host:port` and register it
 * under `name`. Subsequent `yaafc_engine_remote(e, name)` returns the
 * session. The engine owns the session and destroys it at engine
 * shutdown. `host` defaults to 127.0.0.1 when NULL.
 *
 * Idempotent: a second call with the same name closes the previous
 * session before opening the new one — useful for reconnect logic. */
struct yaafc_void_result yaafc_engine_add_remote(struct yaafc_engine *e,
                                                  const char *name,
                                                  const char *host, int port);

/* Borrow the registered session, or NULL if unknown. The session
 * stays owned by the engine. */
struct peer_channel *yaafc_engine_remote(struct yaafc_engine *e, const char *name);

/* Auto-dispatch helper: produce a `struct ctx` for talking to `service`.
 * If the engine has a registered remote with that name, `ctx.peer`
 * is filled in — generated method stubs will RPC. If not (e.g. the
 * service lives in this same process), `ctx.peer` stays NULL and
 * the generated stubs dispatch locally. Either way callers write the
 * same code:
 *
 *     struct ctx c = yaafc_engine_service_ctx(e, "storage");
 *     struct object_ptr_result o = storage_db_create(&c);
 *
 * which removes the manual `c.peer = yaafc_engine_remote(...)`
 * boilerplate at every call site (gh#2). */
struct ctx yaafc_engine_service_ctx(struct yaafc_engine *e, const char *service);

/* Walk the engine's yconfig for `<plugin>.remotes.<idx>.{service,host,port}`
 * entries and call `yaafc_engine_add_remote` for each. Returns the
 * number of remotes opened. Silently skips entries that have no
 * `service:` field or that fail to connect (logged via ywarn).
 *
 * If `host`/`port` are absent in the config, the function looks up
 * `<service>.bind.host` / `<service>.bind.port` at the config root —
 * matching the scenario YAML's `mesh.services.<svc>.host/port` shape. */
size_t yaafc_engine_open_remotes(struct yaafc_engine *e, const char *plugin);

/* Iterate every registered plugin class. The engine walks the class
 * registry's accessor-lookup chain by name pattern — by default,
 * everything registered as a `class@*:*` annotation. Returns names
 * suitable for passing to `class_by_name`. */
struct yaafc_void_result yaafc_engine_for_each_plugin(struct yaafc_engine *e,
                                                     void (*cb)(const char *qname, void *ud),
                                                     void *ud);

#endif /* YAAFC_YENGINE_ENGINE_H */
