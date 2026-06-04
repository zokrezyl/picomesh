/* readlink + sigaction live behind _POSIX_C_SOURCE on glibc; the
 * codegen runs clang without that, so set the minimum at file-top
 * before any include pulls in <features.h>. */
#define _POSIX_C_SOURCE 200809L

/* mesh — orchestrator (skeleton).
 *
 * The yaapp mesh spawns each declared service as a child process and
 * brokers their addresses via portalloc. Subprocess management needs
 * libuv's `uv_spawn` plus shutdown/restart logic — wired in a follow-
 * up patch. For now the plugin exposes the inspection surface so the
 * gateway can call mesh.reconcile / mesh.status without crashing:
 *
 *   register_service(service_id, port)   → 1
 *   resolve(service_id)                  → port (0 if unknown)
 *   forget(service_id)                   → 1 / 0
 *   count_services                       → currently-registered count
 *   reconcile                            → stub: returns 1 */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/yloop/yloop.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/yargv/yargv.h>
#include <picomesh/plugin/storage/storage.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#define MESH_MAX_SERVICES 64
#define MESH_MAX_CHILDREN 32

struct mesh_service_entry {
    uint32_t service_id;
    uint32_t port;
    int used;
};

struct mesh_child_entry {
    int32_t pid;
    uint32_t port;     /* the port we asked the child to bind on */
    int exited;
    int exit_status;
};

struct PICOMESH_CLASS_ANNOTATE("class@mesh:mesh") mesh_mesh_data {
    struct mesh_service_entry entries[MESH_MAX_SERVICES];
    struct mesh_child_entry children[MESH_MAX_CHILDREN];
    size_t count;
    size_t child_count;
};

static struct mesh_mesh_data *ms(struct object *obj)
{
    return (struct mesh_mesh_data *)((char *)obj + sizeof(struct object));
}

/* ---- child lifecycle: the mesh owns and reaps what it spawned ---------- *
 *
 * Two layers, no flat pidfiles and no external pkill:
 *  1. Signal reaper — a SIGTERM/SIGINT handler SIGTERMs every live child this
 *     parent spawned, so `kill -TERM <parent>` takes the whole stack down. A
 *     handler may only touch async-signal-safe state, so live pids are
 *     mirrored in a file-scope table and reaped with kill(2) (which IS
 *     async-signal-safe). This — and the single process-lifetime storage
 *     handle below — are the one sanctioned file-scope use: signal state.
 *  2. Storage record — every spawned pid (and the parent's own pid) is
 *     written via the `storage` plugin under context "mesh". A fresh bring-up
 *     reaps any pid a previous run left alive (clearing restart port races);
 *     operators read the parent pid from storage to signal it, never by
 *     scanning the process table. */

static volatile sig_atomic_t g_mesh_reap_pids[MESH_MAX_CHILDREN];
static volatile sig_atomic_t g_mesh_reap_installed;
static struct object *g_mesh_store_db; /* process-lifetime KV handle for PIDs */

static void mesh_reap_signal_handler(int sig)
{
    for (int i = 0; i < MESH_MAX_CHILDREN; ++i) {
        pid_t p = (pid_t)g_mesh_reap_pids[i];
        if (p > 0) kill(p, SIGTERM); /* async-signal-safe */
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void mesh_reap_install(void)
{
    if (g_mesh_reap_installed) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = mesh_reap_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    g_mesh_reap_installed = 1;
}

/* Create (once) the in-process storage object for PID bookkeeping. Resolves
 * locally because the parent activates the `storage` plugin. */
static struct object *mesh_storage_db(void)
{
    if (g_mesh_store_db) return g_mesh_store_db;
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return NULL;
    struct ctx c = picomesh_engine_service_ctx(e, "storage");
    struct object_ptr_result o = storage_db_create(&c);
    if (PICOMESH_IS_ERR(o)) { picomesh_error_destroy(o.error); return NULL; }
    g_mesh_store_db = o.value;
    return g_mesh_store_db;
}

/* Persist a lifecycle row. Propagates the storage failure so callers that
 * MUST have it durable (e.g. the parent pid) can surface it rather than
 * pretend success. */
static struct picomesh_void_result mesh_storage_set(const char *key, const char *val)
{
    struct object *db = mesh_storage_db();
    struct picomesh_engine *e = picomesh_active_engine();
    if (!db || !e) return PICOMESH_ERR(picomesh_void, "mesh: storage unavailable");
    struct ctx c = picomesh_engine_service_ctx(e, "storage");
    struct picomesh_int_result r = storage_set(&c, db, NULL, "mesh", key, val);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "mesh: lifecycle write failed", r);
    return PICOMESH_OK_VOID();
}

static struct picomesh_void_result mesh_storage_del(const char *key)
{
    struct object *db = mesh_storage_db();
    struct picomesh_engine *e = picomesh_active_engine();
    if (!db || !e) return PICOMESH_ERR(picomesh_void, "mesh: storage unavailable");
    struct ctx c = picomesh_engine_service_ctx(e, "storage");
    struct picomesh_int_result r = storage_del(&c, db, NULL, "mesh", key);
    if (PICOMESH_IS_ERR(r)) return PICOMESH_ERR(picomesh_void, "mesh: lifecycle delete failed", r);
    return PICOMESH_OK_VOID();
}

/* Record a freshly spawned child: signal-reaper mirror + storage row. The
 * in-memory mirror (set first) is what reaps the child on THIS run's
 * shutdown; the storage row is the cross-run backup (reaped after a crash).
 * A failed storage write is therefore non-fatal but must be LOUD, never
 * silent — the caller logs it (the cross-run record is missing). Returns the
 * write result so the spawn path can surface it. */
static struct picomesh_void_result mesh_child_track(int slot, int pid, const char *name)
{
    if (slot >= 0 && slot < MESH_MAX_CHILDREN) g_mesh_reap_pids[slot] = pid;
    char key[32], val[160];
    snprintf(key, sizeof(key), "child:%d", slot);
    snprintf(val, sizeof(val), "%d %s", pid, name ? name : "");
    return mesh_storage_set(key, val);
}

static void mesh_child_untrack(int slot)
{
    if (slot >= 0 && slot < MESH_MAX_CHILDREN) g_mesh_reap_pids[slot] = 0;
    char key[32];
    snprintf(key, sizeof(key), "child:%d", slot);
    /* Best-effort cleanup of the cross-run record; log if it fails. */
    struct picomesh_void_result r = mesh_storage_del(key);
    if (PICOMESH_IS_ERR(r)) {
        ywarn("mesh: failed to clear child record slot=%d (%s)", slot, r.error.msg ? r.error.msg : "?");
        picomesh_error_destroy(r.error);
    }
}

/* Reap children a PREVIOUS run left alive (its parent was SIGKILLed/crashed
 * before the handler ran) and clear the stale records. Called on bring-up. */
static void mesh_reap_previous_run(void)
{
    struct object *db = mesh_storage_db();
    struct picomesh_engine *e = picomesh_active_engine();
    if (!db || !e) return;
    struct ctx c = picomesh_engine_service_ctx(e, "storage");
    for (int slot = 0; slot < MESH_MAX_CHILDREN; ++slot) {
        char key[32];
        snprintf(key, sizeof(key), "child:%d", slot);
        struct picomesh_string_result g = storage_get(&c, db, NULL, "mesh", key);
        if (PICOMESH_IS_OK(g) && g.value && *g.value) {
            int pid = (int)strtol(g.value, NULL, 10);
            if (pid > 0 && kill(pid, 0) == 0) {
                kill(pid, SIGTERM);
                yinfo("mesh: reaped orphan pid=%d from a previous run", pid);
            }
        }
        if (PICOMESH_IS_OK(g)) free(g.value); else picomesh_error_destroy(g.error);
        /* Best-effort clear of the stale record; log if it fails. */
        struct picomesh_void_result d = mesh_storage_del(key);
        if (PICOMESH_IS_ERR(d)) {
            ywarn("mesh: failed to clear stale child record slot=%d (%s)", slot, d.error.msg ? d.error.msg : "?");
            picomesh_error_destroy(d.error);
        }
    }
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_register_service")
struct picomesh_int_result mesh_mesh_register_service_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t service_id, uint32_t port)
{
    (void)ctx;
    struct mesh_mesh_data *d = ms(obj);
    /* idempotent: re-registering same id updates the port */
    for (size_t i = 0; i < MESH_MAX_SERVICES; ++i) {
        if (d->entries[i].used && d->entries[i].service_id == service_id) {
            d->entries[i].port = port;
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    for (size_t i = 0; i < MESH_MAX_SERVICES; ++i) {
        if (!d->entries[i].used) {
            d->entries[i].service_id = service_id;
            d->entries[i].port = port;
            d->entries[i].used = 1;
            d->count++;
            yinfo("mesh: registered service %u on port %u", service_id, port);
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_ERR(picomesh_int, "mesh_register_service: table full");
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_resolve")
struct picomesh_uint32_result mesh_mesh_resolve_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                   uint32_t service_id)
{
    (void)ctx;
    struct mesh_mesh_data *d = ms(obj);
    for (size_t i = 0; i < MESH_MAX_SERVICES; ++i) {
        if (d->entries[i].used && d->entries[i].service_id == service_id) {
            return PICOMESH_OK(picomesh_uint32, d->entries[i].port);
        }
    }
    return PICOMESH_OK(picomesh_uint32, 0);
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_forget")
struct picomesh_int_result mesh_mesh_forget_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                               uint32_t service_id)
{
    (void)ctx;
    struct mesh_mesh_data *d = ms(obj);
    for (size_t i = 0; i < MESH_MAX_SERVICES; ++i) {
        if (d->entries[i].used && d->entries[i].service_id == service_id) {
            d->entries[i].used = 0;
            d->count--;
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_OK(picomesh_int, 0);
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_count_services")
struct picomesh_size_result mesh_mesh_count_services_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return PICOMESH_OK(picomesh_size, ms(obj)->count);
}

/* uv_spawn-based subprocess management:
 *   spawn_picomesh(port)   → child pid (0 on failure)
 *   kill_pid(pid)       → 1 ok / 0 unknown
 *   count_children      → live (not-yet-reaped) child count
 *
 * spawn_picomesh(port) launches a fresh `<self> --frontend yttp --host
 * 127.0.0.1 --port <port> serve`. The child inherits stdin/stdout/
 * stderr from the parent so its trace lands in the parent's terminal.
 *
 * The exit callback flips the matching `exited` flag so kill_pid /
 * count_children stay accurate without a reap step.
 *
 * Path discovery uses `/proc/self/exe` on Linux. On macOS we'd use
 * `_NSGetExecutablePath` — TODO for the platform shim. */

static int find_child_slot(struct mesh_mesh_data *d, int32_t pid)
{
    for (int i = 0; i < MESH_MAX_CHILDREN; ++i) {
        if (!d->children[i].exited && d->children[i].pid == pid) return i;
    }
    return -1;
}

static void mesh_child_exit_cb(struct yloop_process *p, int64_t exit_status,
                               int term_signal, void *ud)
{
    (void)p; (void)term_signal;
    struct object *obj = ud;
    if (!obj) return;
    struct mesh_mesh_data *d = ms(obj);
    /* libuv only knows the uv_process_t pointer; the pid we stashed
     * at spawn time matches the slot. Sweep for any non-exited child
     * whose pid matches the captured exit. Since uv passes us the
     * yloop_process but not the pid directly, we walk all live
     * children and mark the first one matching this callback chain
     * — limitation: assumes one child exits per cb (true), and pid
     * uniqueness in the window. Good enough. */
    /* We can't recover the pid from `p` here without leaking the
     * yloop_process internals, so we use `ud` to carry both obj +
     * pid in a pair. */
    (void)d; (void)exit_status;
}

struct mesh_child_cookie {
    struct object *obj;
    int32_t pid;
};

static void mesh_child_exit_cb_real(struct yloop_process *p, int64_t exit_status,
                                    int term_signal, void *ud)
{
    (void)p;
    struct mesh_child_cookie *c = ud;
    if (!c) return;
    struct mesh_mesh_data *d = ms(c->obj);
    int slot = find_child_slot(d, c->pid);
    if (slot >= 0) {
        d->children[slot].exited = 1;
        d->children[slot].exit_status = (int)exit_status;
        d->child_count--;
        mesh_child_untrack(slot);
        yinfo("mesh: child pid=%d exited (status=%d, term_signal=%d)",
              c->pid, (int)exit_status, term_signal);
    }
    free(c);
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_spawn_picomesh")
struct picomesh_int_result mesh_mesh_spawn_picomesh_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                    uint32_t port)
{
    (void)ctx;
    struct mesh_mesh_data *d = ms(obj);
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(picomesh_int, "spawn_picomesh: no active engine");

    int slot = -1;
    for (int i = 0; i < MESH_MAX_CHILDREN; ++i) {
        if (d->children[i].exited || d->children[i].pid == 0) { slot = i; break; }
    }
    if (slot < 0) return PICOMESH_ERR(picomesh_int, "spawn_picomesh: child table full");

    /* /proc/self/exe is Linux-only; macOS needs _NSGetExecutablePath
     * (TODO via yplatform once a Mac build is exercised). */
    char self_exe[4096];
    ssize_t n = readlink("/proc/self/exe", self_exe, sizeof(self_exe) - 1);
    if (n <= 0) return PICOMESH_ERR(picomesh_int, "spawn_picomesh: readlink(/proc/self/exe) failed");
    self_exe[n] = 0;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);

    char *argv[] = {
        self_exe,
        (char *)"--frontend", (char *)"yhttp",
        (char *)"--host", (char *)"127.0.0.1",
        (char *)"--port", port_str,
        (char *)"serve",
        NULL,
    };

    struct mesh_child_cookie *c = calloc(1, sizeof(*c));
    if (!c) return PICOMESH_ERR(picomesh_int, "spawn_picomesh: calloc(cookie) failed");
    c->obj = obj;

    int pid = yloop_spawn(picomesh_engine_loop(e), self_exe, argv,
                          mesh_child_exit_cb_real, c);
    if (pid <= 0) {
        free(c);
        return PICOMESH_ERR(picomesh_int, "spawn_picomesh: yloop_spawn failed");
    }
    c->pid = pid;
    d->children[slot].pid = pid;
    d->children[slot].port = port;
    d->children[slot].exited = 0;
    d->children[slot].exit_status = 0;
    d->child_count++;
    mesh_reap_install();
    struct picomesh_void_result tr = mesh_child_track(slot, pid, "picomesh");
    if (PICOMESH_IS_ERR(tr)) {
        ywarn("mesh: spawned pid=%d but its cross-run reap record FAILED to persist (%s) — "
              "the in-memory reaper still covers this run's shutdown", pid, tr.error.msg ? tr.error.msg : "?");
        picomesh_error_destroy(tr.error);
    }
    yinfo("mesh: spawned pid=%d on port=%u", pid, port);
    return PICOMESH_OK(picomesh_int, pid);
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_kill_pid")
struct picomesh_int_result mesh_mesh_kill_pid_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                 int32_t pid)
{
    (void)ctx;
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(picomesh_int, "kill_pid: no active engine");
    struct mesh_mesh_data *d = ms(obj);
    if (find_child_slot(d, pid) < 0) {
        return PICOMESH_OK(picomesh_int, 0);
    }
    int rc = yloop_kill(picomesh_engine_loop(e), pid, SIGTERM);
    return PICOMESH_OK(picomesh_int, rc == 0 ? 1 : 0);
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_count_children")
struct picomesh_size_result mesh_mesh_count_children_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return PICOMESH_OK(picomesh_size, ms(obj)->child_count);
}

/* for_each cb state: emit "<key>: <value>\n" for each entry of the node's
 * `config:` map at top level (column 0). `value` is serialized with the
 * shared node dumper, so nested maps/lists round-trip. */
struct split_emit { char *buf; size_t off; size_t cap; int overflow; };

static int split_config_kv_cb(const char *key, const struct yconfig_node *val, void *ud)
{
    struct split_emit *se = ud;
    int kn = snprintf(se->buf + se->off, se->cap - se->off, "%s: ", key);
    if (kn < 0 || (size_t)kn >= se->cap - se->off) { se->overflow = 1; return 1; }
    se->off += (size_t)kn;
    size_t vn = yconfig_node_dump(val, se->buf + se->off, se->cap - se->off);
    se->off += vn;
    if (se->off + 2 >= se->cap) { se->overflow = 1; return 1; }
    se->buf[se->off++] = '\n';
    se->buf[se->off] = 0;
    return 0;
}

/* Split one service's config out of the mesh config into a STANDALONE
 * per-node config file and return its path (written into `out`).
 *
 * The mesh config is the mesh plugin's OWN config: it lists nodes, their
 * ports, and each node's `config:` block. A node must NOT read the mesh
 * config — it gets its own file. So at spawn time we lift
 * `mesh.services.<name>.config` (the node's plugins + per-plugin config +
 * remotes) to the TOP LEVEL of a fresh file, plus the node's own `name:`,
 * `host:`, `port:`, `frontend:`. The child is launched with just
 * `--config-file <that file>` and finds everything at natural top-level
 * paths — no `--name` projection, no mesh.services lookup.
 *
 * Returns 0 on success. Files land under <dir>/nodes/<name>.yaml where
 * <dir> is the mesh's working area (derives from /tmp/picoforge). */
static int mesh_write_node_config(struct picomesh_engine *e, const char *name,
                                  char *out, size_t out_cap)
{
    char base[256];
    snprintf(base, sizeof(base), "mesh.services.%s", name);

    /* Pull the per-node pieces from the mesh config. */
    char path[320];
    snprintf(path, sizeof(path), "%s.config", base);
    struct yconfig_node_ptr_result cfgr = yconfig_get(picomesh_engine_config(e), path);
    const struct yconfig_node *cfg_node =
        (PICOMESH_IS_OK(cfgr) && cfgr.value) ? cfgr.value : NULL;

    snprintf(path, sizeof(path), "%s.port", base);
    struct yconfig_node_ptr_result pr = yconfig_get(picomesh_engine_config(e), path);
    const struct yconfig_node *port_node = (PICOMESH_IS_OK(pr) && pr.value) ? pr.value : NULL;
    int64_t port = -1;
    int port_auto = 0;
    if (port_node) {
        if (yconfig_node_kind(port_node) == YCONFIG_STRING) {
            const char *ps = yconfig_node_as_string(port_node, NULL);
            port_auto = (ps && strcmp(ps, "auto") == 0);
        } else {
            port = yconfig_node_as_int(port_node, -1);
        }
    }

    snprintf(path, sizeof(path), "%s.host", base);
    struct yconfig_node_ptr_result hr = yconfig_get(picomesh_engine_config(e), path);
    const char *host = (PICOMESH_IS_OK(hr) && hr.value) ? yconfig_node_as_string(hr.value, NULL) : NULL;

    snprintf(path, sizeof(path), "%s.frontend", base);
    struct yconfig_node_ptr_result fr = yconfig_get(picomesh_engine_config(e), path);
    const char *frontend = (PICOMESH_IS_OK(fr) && fr.value) ? yconfig_node_as_string(fr.value, NULL) : NULL;

    snprintf(path, sizeof(path), "%s.plugins", base);
    struct yconfig_node_ptr_result plr = yconfig_get(picomesh_engine_config(e), path);
    const struct yconfig_node *plugins_node =
        (PICOMESH_IS_OK(plr) && plr.value) ? plr.value : NULL;

    snprintf(path, sizeof(path), "%s.workers", base);
    struct yconfig_node_ptr_result wr = yconfig_get(picomesh_engine_config(e), path);
    int64_t workers = (PICOMESH_IS_OK(wr) && wr.value) ? yconfig_node_as_int(wr.value, -1) : -1;

    /* Build the standalone node file as flow YAML (yconfig parses it). */
    char body[16384];
    size_t off = 0;
    off += (size_t)snprintf(body + off, sizeof(body) - off, "name: \"%s\"\n", name);
    if (host)     off += (size_t)snprintf(body + off, sizeof(body) - off, "host: \"%s\"\n", host);
    /* `port: auto` is passed through verbatim — the child resolves it via
     * portalloc at serve time. A numeric port is baked in as before. */
    if (port_auto)    off += (size_t)snprintf(body + off, sizeof(body) - off, "port: auto\n");
    else if (port > 0) off += (size_t)snprintf(body + off, sizeof(body) - off, "port: %lld\n", (long long)port);
    if (frontend) off += (size_t)snprintf(body + off, sizeof(body) - off, "frontend: \"%s\"\n", frontend);
    if (workers > 0) off += (size_t)snprintf(body + off, sizeof(body) - off, "workers: %lld\n", (long long)workers);

    /* Inject the registry's fixed address as a global block so the child can
     * reach the registry (and, through it, portalloc and its auto remotes)
     * before it knows where anything else lives. Sourced from the registry
     * service's own config — the single place its port is pinned; overriding
     * that one key is how a second instance avoids colliding. */
    struct yconfig_node_ptr_result reg_hr =
        yconfig_get(picomesh_engine_config(e), "mesh.services.registry.host");
    struct yconfig_node_ptr_result reg_pr =
        yconfig_get(picomesh_engine_config(e), "mesh.services.registry.port");
    int64_t reg_port = (PICOMESH_IS_OK(reg_pr) && reg_pr.value)
                           ? yconfig_node_as_int(reg_pr.value, -1) : -1;
    if (reg_port > 0) {
        const char *reg_host = (PICOMESH_IS_OK(reg_hr) && reg_hr.value)
                                   ? yconfig_node_as_string(reg_hr.value, NULL) : NULL;
        off += (size_t)snprintf(body + off, sizeof(body) - off,
                                "registry:\n  host: \"%s\"\n  port: %lld\n",
                                (reg_host && *reg_host) ? reg_host : "127.0.0.1",
                                (long long)reg_port);
    }
    if (plugins_node) {
        off += (size_t)snprintf(body + off, sizeof(body) - off, "plugins: ");
        off += yconfig_node_dump(plugins_node, body + off, sizeof(body) - off);
        off += (size_t)snprintf(body + off, sizeof(body) - off, "\n");
    }
    /* The node's `config:` block — its per-plugin config + remotes —
     * lifted to top level: emit each of its keys at column 0 so plugins
     * read them at natural paths (`git_repo.repos_dir`, `remotes`, …). */
    if (cfg_node && yconfig_node_kind(cfg_node) == YCONFIG_MAP) {
        struct split_emit se = {body, off, sizeof(body), 0};
        yconfig_node_for_each(cfg_node, split_config_kv_cb, &se);
        off = se.off;
        if (se.overflow) return -1;
    }

    if (off >= sizeof(body)) return -1;

    /* Path: alongside the parent's repos tree, under nodes/. */
    snprintf(out, out_cap, "/tmp/picoforge/nodes/%s.yaml", name);
    /* mkdir -p /tmp/picoforge/nodes */
    char dir[256]; snprintf(dir, sizeof(dir), "/tmp/picoforge/nodes");
    char acc[256] = {0};
    for (size_t i = 1; dir[i]; ++i) {
        if (dir[i] == '/') { memcpy(acc, dir, i); acc[i] = 0; mkdir(acc, 0755); }
    }
    mkdir(dir, 0755);

    FILE *f = fopen(out, "w");
    if (!f) { ywarn("mesh.split: cannot write %s: %s", out, strerror(errno)); return -1; }
    fwrite(body, 1, off, f);
    fclose(f);
    return 0;
}

/* Spawn one child picomesh process for service `name`. The mesh SPLITS its
 * config into a standalone per-node file (mesh_write_node_config) and the
 * child is launched with just that file:
 *
 *   picomesh --config-file <nodes/<name>.yaml> --name <name> --frontend X serve
 *
 * The node file carries the node's own top-level config (plugins, per-
 * plugin config, remotes, port, host, frontend) — the child never reads
 * the mesh config. `--name` is kept only so the engine resolves bind
 * port/host from the node file's top-level keys uniformly. */
static int mesh_internal_spawn(struct object *obj, const char *name)
{
    if (!name || !*name) return 0;
    struct mesh_mesh_data *d = ms(obj);
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return 0;
    int slot = -1;
    for (int i = 0; i < MESH_MAX_CHILDREN; ++i) {
        if (d->children[i].exited || d->children[i].pid == 0) { slot = i; break; }
    }
    if (slot < 0) return 0;
    char self_exe[4096];
    ssize_t n = readlink("/proc/self/exe", self_exe, sizeof(self_exe) - 1);
    if (n <= 0) return 0;
    self_exe[n] = 0;

    /* Per CLAUDE.md: backends listen yrpc by default; only the gateway
     * opts into yhttp via `frontend: yhttp`. Read it from the mesh config
     * (we still have it here, in the parent). */
    char fe_path[256];
    snprintf(fe_path, sizeof(fe_path), "mesh.services.%s.frontend", name);
    const char *frontend = "yrpc";
    struct yconfig_node_ptr_result fr = yconfig_get(picomesh_engine_config(e), fe_path);
    if (PICOMESH_IS_OK(fr) && fr.value) {
        const char *s = yconfig_node_as_string(fr.value, NULL);
        if (s && *s) frontend = s;
    }

    /* Split the mesh config → standalone per-node config file. */
    static char node_cfg[MESH_MAX_CHILDREN][256];
    if (mesh_write_node_config(e, name, node_cfg[slot], sizeof(node_cfg[slot])) != 0) {
        ywarn("mesh.spawn: failed to write per-node config for '%s'", name);
        return 0;
    }
    const char *cfg = node_cfg[slot];

    /* `cfg`, `name`, and `frontend` outlive the spawn call (cfg is in the
     * process-lifetime node_cfg table), so dropping them into argv[] is safe. */
    char *argv[] = {
        self_exe,
        (char *)"--config-file", (char *)cfg,
        (char *)"--name",        (char *)name,
        (char *)"--frontend",    (char *)frontend,
        (char *)"serve",
        NULL,
    };

    struct mesh_child_cookie *c = calloc(1, sizeof(*c));
    if (!c) return 0;
    c->obj = obj;
    int pid = yloop_spawn(picomesh_engine_loop(e), self_exe, argv,
                          mesh_child_exit_cb_real, c);
    if (pid <= 0) { free(c); return 0; }
    c->pid = pid;
    d->children[slot].pid = pid;
    d->children[slot].port = 0; /* port is in the YAML; child resolves */
    d->children[slot].exited = 0;
    d->child_count++;
    mesh_reap_install();
    struct picomesh_void_result tr = mesh_child_track(slot, pid, name);
    if (PICOMESH_IS_ERR(tr)) {
        ywarn("mesh: spawned service='%s' pid=%d but its cross-run reap record FAILED to persist (%s) — "
              "the in-memory reaper still covers this run's shutdown", name, pid, tr.error.msg ? tr.error.msg : "?");
        picomesh_error_destroy(tr.error);
    }
    yinfo("mesh: spawned pid=%d service='%s'", pid, name);
    return pid;
}

/* Walk callback for the services map. Each iteration sees one
 * (service_name, service_node) pair — we always spawn a child for
 * each service (the child looks up its own port/host/remotes by name
 * via the inherited --config-file). */
struct reconcile_ctx {
    struct object *obj;
    int spawned;
};

/* The discovery plane comes up first and is spawned explicitly, so the walk
 * must not spawn these a second time. */
static int reconcile_is_discovery_plane(const char *service_name)
{
    return strcmp(service_name, "registry") == 0 || strcmp(service_name, "portalloc") == 0;
}

static int reconcile_walk_cb(const char *service_name,
                             const struct yconfig_node *val, void *ud)
{
    struct reconcile_ctx *rc = ud;
    if (yconfig_node_kind(val) != YCONFIG_MAP) return 0;
    if (reconcile_is_discovery_plane(service_name)) return 0; /* already spawned */

    int pid = mesh_internal_spawn(rc->obj, service_name);
    if (pid > 0) {
        rc->spawned++;
        yinfo("mesh.reconcile: '%s' → pid=%d", service_name, pid);
    } else {
        ywarn("mesh.reconcile: failed to spawn '%s'", service_name);
    }
    return 0;
}

/* Spawn one named service if it exists in mesh.services. Used to bring the
 * discovery plane (registry, then portalloc) up before everything else. */
static void reconcile_spawn_named(struct reconcile_ctx *rc, struct picomesh_engine *e,
                                  const char *name)
{
    char path[160];
    snprintf(path, sizeof(path), "mesh.services.%s", name);
    struct yconfig_node_ptr_result r = yconfig_get(picomesh_engine_config(e), path);
    if (PICOMESH_IS_ERR(r) || !r.value || yconfig_node_kind(r.value) != YCONFIG_MAP) return;
    int pid = mesh_internal_spawn(rc->obj, name);
    if (pid > 0) {
        rc->spawned++;
        yinfo("mesh.reconcile: '%s' (discovery plane) → pid=%d", name, pid);
    } else {
        ywarn("mesh.reconcile: failed to spawn discovery-plane service '%s'", name);
    }
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_reconcile_from_config")
struct picomesh_int_result mesh_mesh_reconcile_from_config_impl(struct ctx *ctx,
                                                              struct object *obj,
                                                              struct yheaders *hdrs)
{
    (void)ctx;
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return PICOMESH_ERR(picomesh_int, "reconcile_from_config: no active engine");

    /* Own the lifecycle: install the SIGTERM/SIGINT reaper, take down any
     * children a previous run left alive, and record this parent's pid in
     * storage so the stack can be stopped by signalling it (no pkill). */
    mesh_reap_install();
    mesh_reap_previous_run();
    char parent_pid[16];
    snprintf(parent_pid, sizeof(parent_pid), "%d", (int)getpid());
    /* The parent pid MUST be durable: it is how the stack is stopped (signal
     * the recorded parent; no pkill). If we can't persist it, refuse to spawn
     * — a running stack with no recorded parent can't be cleanly taken down. */
    struct picomesh_void_result pp = mesh_storage_set("parent", parent_pid);
    if (PICOMESH_IS_ERR(pp))
        return PICOMESH_ERR(picomesh_int, "reconcile_from_config: cannot persist parent pid", pp);

    /* `mesh.services` — a map keyed by service name. */
    struct yconfig_node_ptr_result r =
        yconfig_get(picomesh_engine_config(e), "mesh.services");
    if (PICOMESH_IS_ERR(r) || !r.value) {
        return PICOMESH_ERR(picomesh_int, "reconcile_from_config: mesh.services missing");
    }
    if (yconfig_node_kind(r.value) != YCONFIG_MAP) {
        return PICOMESH_ERR(picomesh_int, "reconcile_from_config: mesh.services not a map");
    }
    struct reconcile_ctx rc = {.obj = obj, .spawned = 0};
    /* Discovery plane first: the registry (fixed address) so nodes can
     * register/discover, then portalloc so `port: auto` nodes can allocate.
     * Children retry-connect while these finish binding, so no barrier is
     * needed — just a head start. The walk then skips both. */
    reconcile_spawn_named(&rc, e, "registry");
    reconcile_spawn_named(&rc, e, "portalloc");
    yconfig_node_for_each(r.value, reconcile_walk_cb, &rc);
    yinfo("mesh.reconcile_from_config: spawned %d services", rc.spawned);
    return PICOMESH_OK(picomesh_int, rc.spawned);
}

PICOMESH_CLASS_ANNOTATE("override@mesh:mesh:mesh_reconcile")
struct picomesh_int_result mesh_mesh_reconcile_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    /* No actual orchestration yet — placeholder for the spawn loop. */
    yinfo("mesh: reconcile (count=%zu)", ms(obj)->count);
    return PICOMESH_OK(picomesh_int, 1);
}

#include "store.gen.c"
