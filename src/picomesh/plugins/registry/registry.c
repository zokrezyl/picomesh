/* registry — service registry for mesh discovery.
 *
 * In-memory `service_name → {instance_id, host, port}` map. Services
 * register their bound address here; consumers look a service up by name
 * to discover its current host:port. Pure data store — no health checks,
 * no persistence (state is lost on restart, by design; the mesh re-spawns
 * and every node re-registers on boot).
 *
 * The registry is one of only TWO nodes with a FIXED address (the other is
 * the mesh control parent). Its address is injected into every child's
 * config as a global block, so a freshly spawned node can reach the
 * registry before it knows where anything else lives — including portalloc.
 *
 * Methods:
 *   register_service(name, instance_id, host, port) → 1
 *   deregister_service(name, instance_id)           → 1 ok / 0 unknown
 *   resolve(name)                                   → "host:port" ("" if unknown)
 *   discover_service(name)                          → JSON {service_name, instances:[…]}
 *   list_services()                                 → JSON [{service_name, instances:[…]}]
 *   count()                                         → number of live instances
 *
 * `resolve` is the framework convenience: a node opening a remote with
 * `port: auto` calls it to turn a service name into a concrete address.
 * The richer JSON methods mirror yaapp's registry for the describe surface. */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yjson/yjson.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGISTRY_MAX_ENTRIES 256
#define REGISTRY_NAME_MAX 64
#define REGISTRY_INSTANCE_MAX 96
#define REGISTRY_HOST_MAX 64

struct registry_entry {
    char name[REGISTRY_NAME_MAX];
    char instance_id[REGISTRY_INSTANCE_MAX];
    char host[REGISTRY_HOST_MAX];
    uint32_t port;
    int used;
};

struct PICOMESH_CLASS_ANNOTATE("class@registry:registry") registry_registry_data {
    struct registry_entry entries[REGISTRY_MAX_ENTRIES];
    size_t count;
};

static struct registry_registry_data *reg(struct object *obj)
{
    return (struct registry_registry_data *)((char *)obj + sizeof(struct object));
}

/* Find the slot holding (name, instance_id), or -1. */
static int registry_find(struct registry_registry_data *data, const char *name,
                         const char *instance_id)
{
    for (size_t i = 0; i < REGISTRY_MAX_ENTRIES; ++i) {
        if (data->entries[i].used &&
            strcmp(data->entries[i].name, name) == 0 &&
            strcmp(data->entries[i].instance_id, instance_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

PICOMESH_CLASS_ANNOTATE("override@registry:registry:registry_register_service")
struct picomesh_int_result registry_registry_register_service_impl(struct ctx *ctx, struct object *obj,
                                                                   struct yheaders *hdrs,
                                                                   const char *name, const char *instance_id,
                                                                   const char *host, uint32_t port)
{
    (void)ctx; (void)hdrs;
    if (!name || !*name) return PICOMESH_ERR(picomesh_int, "registry_register: empty service name");
    if (!instance_id || !*instance_id) instance_id = name;
    if (!host || !*host) host = "127.0.0.1";
    struct registry_registry_data *data = reg(obj);

    /* Re-registering (name, instance_id) refreshes its address. */
    int slot = registry_find(data, name, instance_id);
    if (slot < 0) {
        for (size_t i = 0; i < REGISTRY_MAX_ENTRIES; ++i) {
            if (!data->entries[i].used) { slot = (int)i; break; }
        }
        if (slot < 0) return PICOMESH_ERR(picomesh_int, "registry_register: table full");
        data->entries[slot].used = 1;
        data->count++;
    }
    snprintf(data->entries[slot].name, sizeof(data->entries[slot].name), "%s", name);
    snprintf(data->entries[slot].instance_id, sizeof(data->entries[slot].instance_id), "%s", instance_id);
    snprintf(data->entries[slot].host, sizeof(data->entries[slot].host), "%s", host);
    data->entries[slot].port = port;
    yinfo("registry: '%s' instance '%s' → %s:%u", name, instance_id, host, port);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@registry:registry:registry_deregister_service")
struct picomesh_int_result registry_registry_deregister_service_impl(struct ctx *ctx, struct object *obj,
                                                                     struct yheaders *hdrs,
                                                                     const char *name, const char *instance_id)
{
    (void)ctx; (void)hdrs;
    if (!name || !*name) return PICOMESH_OK(picomesh_int, 0);
    if (!instance_id || !*instance_id) instance_id = name;
    struct registry_registry_data *data = reg(obj);
    int slot = registry_find(data, name, instance_id);
    if (slot < 0) return PICOMESH_OK(picomesh_int, 0);
    data->entries[slot].used = 0;
    data->count--;
    yinfo("registry: deregistered '%s' instance '%s'", name, instance_id);
    return PICOMESH_OK(picomesh_int, 1);
}

/* Resolve a service name to a concrete "host:port" string — the framework's
 * port: auto edge. Returns the most-recently-registered live instance. An
 * unknown service yields an empty string (the caller retries until the
 * producer has registered), NOT an error. */
PICOMESH_CLASS_ANNOTATE("override@registry:registry:registry_resolve")
struct picomesh_string_result registry_registry_resolve_impl(struct ctx *ctx, struct object *obj,
                                                             struct yheaders *hdrs, const char *name)
{
    (void)ctx; (void)hdrs;
    struct registry_registry_data *data = reg(obj);
    const struct registry_entry *hit = NULL;
    if (name && *name) {
        for (size_t i = 0; i < REGISTRY_MAX_ENTRIES; ++i) {
            if (data->entries[i].used && strcmp(data->entries[i].name, name) == 0) {
                hit = &data->entries[i]; /* keep scanning: last write wins */
            }
        }
    }
    char buf[REGISTRY_HOST_MAX + 16];
    if (hit) snprintf(buf, sizeof(buf), "%s:%u", hit->host, hit->port);
    else buf[0] = 0;
    char *out = strdup(buf);
    if (!out) return PICOMESH_ERR(picomesh_string, "registry_resolve: out of memory");
    return PICOMESH_OK(picomesh_string, out);
}

/* Emit the instances of `name` as a JSON array onto an open writer. */
static void registry_write_instances(struct yjson_writer *writer,
                                     struct registry_registry_data *data, const char *name)
{
    yjson_writer_begin_array(writer);
    for (size_t i = 0; i < REGISTRY_MAX_ENTRIES; ++i) {
        if (!data->entries[i].used || strcmp(data->entries[i].name, name) != 0) continue;
        yjson_writer_begin_object(writer);
        yjson_writer_key(writer, "instance_id"); yjson_writer_string(writer, data->entries[i].instance_id);
        yjson_writer_key(writer, "host");        yjson_writer_string(writer, data->entries[i].host);
        yjson_writer_key(writer, "port");        yjson_writer_int(writer, (int64_t)data->entries[i].port);
        yjson_writer_end_object(writer);
    }
    yjson_writer_end_array(writer);
}

PICOMESH_CLASS_ANNOTATE("override@registry:registry:registry_discover_service")
struct picomesh_json_result registry_registry_discover_service_impl(struct ctx *ctx, struct object *obj,
                                                                    struct yheaders *hdrs, const char *name)
{
    (void)ctx; (void)hdrs;
    if (!name) name = "";
    struct registry_registry_data *data = reg(obj);
    size_t total = 0;
    for (size_t i = 0; i < REGISTRY_MAX_ENTRIES; ++i) {
        if (data->entries[i].used && strcmp(data->entries[i].name, name) == 0) total++;
    }
    struct yjson_writer *writer = yjson_writer_new();
    if (!writer) return PICOMESH_ERR(picomesh_json, "registry_discover: writer alloc failed");
    yjson_writer_begin_object(writer);
    yjson_writer_key(writer, "service_name"); yjson_writer_string(writer, name);
    yjson_writer_key(writer, "instances");    registry_write_instances(writer, data, name);
    yjson_writer_key(writer, "total_instances"); yjson_writer_int(writer, (int64_t)total);
    yjson_writer_end_object(writer);
    size_t len = 0;
    const char *jdata = yjson_writer_data(writer, &len);
    char *out = strdup(jdata ? jdata : "{}");
    yjson_writer_free(writer);
    if (!out) return PICOMESH_ERR(picomesh_json, "registry_discover: out of memory");
    return PICOMESH_OK(picomesh_json, out);
}

PICOMESH_CLASS_ANNOTATE("override@registry:registry:registry_list_services")
struct picomesh_json_result registry_registry_list_services_impl(struct ctx *ctx, struct object *obj,
                                                                 struct yheaders *hdrs)
{
    (void)ctx; (void)hdrs;
    struct registry_registry_data *data = reg(obj);
    struct yjson_writer *writer = yjson_writer_new();
    if (!writer) return PICOMESH_ERR(picomesh_json, "registry_list: writer alloc failed");
    yjson_writer_begin_array(writer);
    /* One object per distinct service name. A name is "first seen" at the
     * lowest slot that carries it; emit there so each name appears once. */
    for (size_t i = 0; i < REGISTRY_MAX_ENTRIES; ++i) {
        if (!data->entries[i].used) continue;
        int first = 1;
        for (size_t j = 0; j < i; ++j) {
            if (data->entries[j].used && strcmp(data->entries[j].name, data->entries[i].name) == 0) {
                first = 0; break;
            }
        }
        if (!first) continue;
        yjson_writer_begin_object(writer);
        yjson_writer_key(writer, "service_name"); yjson_writer_string(writer, data->entries[i].name);
        yjson_writer_key(writer, "instances");    registry_write_instances(writer, data, data->entries[i].name);
        yjson_writer_end_object(writer);
    }
    yjson_writer_end_array(writer);
    size_t len = 0;
    const char *jdata = yjson_writer_data(writer, &len);
    char *out = strdup(jdata ? jdata : "[]");
    yjson_writer_free(writer);
    if (!out) return PICOMESH_ERR(picomesh_json, "registry_list: out of memory");
    return PICOMESH_OK(picomesh_json, out);
}

PICOMESH_CLASS_ANNOTATE("override@registry:registry:registry_count")
struct picomesh_size_result registry_registry_count_impl(struct ctx *ctx, struct object *obj,
                                                         struct yheaders *hdrs)
{
    (void)ctx; (void)hdrs;
    return PICOMESH_OK(picomesh_size, reg(obj)->count);
}

#include "registry.gen.c"
