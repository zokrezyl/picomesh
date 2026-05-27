/* yconfig — hierarchical YAML configuration.
 *
 * Port of yaapp's `lib/config.py`. Same precedence + same lookup
 * semantics; libyaml under the hood.
 *
 * Precedence, lowest to highest (later entries override earlier
 * entries for the same key):
 *
 *   1. defaults provided at create time (a dict, by convention the
 *      CLI `--config K=V` overrides — they're applied first, then
 *      every file can override them; this matches the yaapp call
 *      `Config.create(defaults=config_overrides, path=config_file)`).
 *   2. $HOME/.config/yaafc/yaafc.yaml
 *   3. <git_repo_root>/yaafc.yaml
 *   4. ./yaafc.yaml
 *   5. explicit path passed to yconfig_create.
 *
 * String values are post-processed with ${VAR} / ${VAR:default}
 * environment-variable substitution, applied recursively.
 *
 * Lookups use dot-notation paths (`server.host`, `storage.backend`).
 * If a key is missing in a nested subtree, the lookup falls back to
 * the parent dict at the *root* level — yaapp's
 * "ConfigNode inheritance". E.g. `storage.host` will return the
 * top-level `host` if `storage` doesn't have one.
 *
 * Every fallible entry point returns a Result. */

#ifndef YAAFC_YCONFIG_YCONFIG_H
#define YAAFC_YCONFIG_YCONFIG_H

#include <yaafc/ycore/result.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct yconfig;
struct yconfig_node;

YAAFC_RESULT_DECLARE(yconfig_ptr, struct yconfig *);
YAAFC_RESULT_DECLARE(yconfig_node_ptr, const struct yconfig_node *);

enum yconfig_kind {
    YCONFIG_NULL,
    YCONFIG_BOOL,
    YCONFIG_INT,
    YCONFIG_FLOAT,
    YCONFIG_STRING,
    YCONFIG_LIST,
    YCONFIG_MAP,
};

struct yconfig_create_args {
    /* Optional explicit config file (highest precedence). */
    const char *config_file;

    /* Optional list of "key=value" CLI overrides (lowest precedence —
     * applied as defaults, then every file can override). */
    const char *const *cli_overrides;
    size_t cli_override_count;

    /* App name decides the XDG path: ~/.config/<app_name>/<app_name>.yaml
     * and the project-root filename (<app_name>.yaml). Defaults to "yaafc"
     * when NULL. */
    const char *app_name;

    /* Skip the host-filesystem search (XDG / project / cwd). Useful for
     * unit tests that want a hermetic config from `defaults` + an explicit
     * `config_file` only. */
    int no_filesystem_search;
};

struct yconfig_ptr_result yconfig_create(const struct yconfig_create_args *args);
void yconfig_destroy(struct yconfig *c);

/* --- lookups --------------------------------------------------------- */

/* Return the root node (the whole config tree). Owned by the yconfig
 * instance — do not free. */
const struct yconfig_node *yconfig_root(const struct yconfig *c);

/* Resolve a dot-path against the root. Inheritance kicks in: when
 * `<head>.<rest>` is missing, retry as `<rest>` against the root.
 * Returns NULL inside the result (not an error) when the path is not
 * present anywhere. */
struct yconfig_node_ptr_result yconfig_get(const struct yconfig *c, const char *dot_path);

/* Sub-tree shortcut for plugins:
 *
 *   const struct yconfig_node *sub = yconfig_section(c, "storage");
 *
 * Returns the top-level `<name>` subtree or NULL if absent. Equivalent
 * to `yaapp_engine.get_config('<name>')`. */
const struct yconfig_node *yconfig_section(const struct yconfig *c, const char *name);

/* --- typed accessors ------------------------------------------------- */

enum yconfig_kind yconfig_node_kind(const struct yconfig_node *n);
size_t yconfig_node_size(const struct yconfig_node *n); /* map/list count */

/* Scalar getters. They walk the inheritance chain only when called
 * via yconfig_get; once you have a node, they read straight off it.
 * Each returns the supplied `fallback` for kind mismatch / NULL node. */
const char *yconfig_node_as_string(const struct yconfig_node *n, const char *fallback);
int64_t yconfig_node_as_int(const struct yconfig_node *n, int64_t fallback);
double yconfig_node_as_float(const struct yconfig_node *n, double fallback);
int yconfig_node_as_bool(const struct yconfig_node *n, int fallback);

/* Map iteration — invokes cb for each (key, value) pair in source
 * order. Stop early by returning non-zero from cb. */
int yconfig_node_for_each(const struct yconfig_node *n,
                          int (*cb)(const char *key, const struct yconfig_node *val, void *ud),
                          void *ud);

/* List iteration. */
const struct yconfig_node *yconfig_node_at(const struct yconfig_node *n, size_t idx);

/* Pretty-print for debugging. Returns the number of bytes written (no
 * trailing NUL). */
size_t yconfig_dump(const struct yconfig *c, char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* YAAFC_YCONFIG_YCONFIG_H */
