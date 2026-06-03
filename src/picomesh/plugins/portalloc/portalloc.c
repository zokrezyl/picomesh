/* portalloc — port allocator.
 *
 * The mesh assigns this plugin a fixed port (8200 in the scenario);
 * every other service asks portalloc for a free port at spawn time.
 *
 * Methods:
 *   allocate(service_id) → uint32 port (0 on full)
 *   release(port)        → 1 ok, 0 unknown
 *   count_used()         → size of in-use set
 *
 * `service_id` is the requester's stable id. Calling allocate twice
 * with the same id returns the same port — idempotent across crashes
 * once the persistence file is wired up (TODO). */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yjson/yjson.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PORTALLOC_RANGE_LO 8201
#define PORTALLOC_RANGE_HI 8299
#define PORTALLOC_MAX_ENTRIES ((PORTALLOC_RANGE_HI - PORTALLOC_RANGE_LO) + 1)

struct port_entry {
    uint32_t service_id;
    uint32_t port;
    int used;
};

struct PICOMESH_CLASS_ANNOTATE("class@portalloc:portalloc") portalloc_portalloc_data {
    struct port_entry entries[PORTALLOC_MAX_ENTRIES];
    size_t count;
};

static struct portalloc_portalloc_data *pa(struct object *obj)
{
    return (struct portalloc_portalloc_data *)((char *)obj + sizeof(struct object));
}

PICOMESH_CLASS_ANNOTATE("override@portalloc:portalloc:portalloc_allocate")
struct picomesh_uint32_result portalloc_portalloc_allocate_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                         uint32_t service_id)
{
    (void)ctx;
    struct portalloc_portalloc_data *d = pa(obj);
    /* idempotent: already-allocated → same port */
    for (size_t i = 0; i < PORTALLOC_MAX_ENTRIES; ++i) {
        if (d->entries[i].used && d->entries[i].service_id == service_id) {
            return PICOMESH_OK(picomesh_uint32, d->entries[i].port);
        }
    }
    /* first-fit on the range */
    for (uint32_t p = PORTALLOC_RANGE_LO; p <= PORTALLOC_RANGE_HI; ++p) {
        int taken = 0;
        for (size_t i = 0; i < PORTALLOC_MAX_ENTRIES; ++i) {
            if (d->entries[i].used && d->entries[i].port == p) { taken = 1; break; }
        }
        if (taken) continue;
        for (size_t i = 0; i < PORTALLOC_MAX_ENTRIES; ++i) {
            if (!d->entries[i].used) {
                d->entries[i].service_id = service_id;
                d->entries[i].port = p;
                d->entries[i].used = 1;
                d->count++;
                yinfo("portalloc: service %u → port %u", service_id, p);
                return PICOMESH_OK(picomesh_uint32, p);
            }
        }
    }
    return PICOMESH_ERR(picomesh_uint32, "portalloc_allocate: no ports left in range");
}

PICOMESH_CLASS_ANNOTATE("override@portalloc:portalloc:portalloc_release")
struct picomesh_int_result portalloc_portalloc_release_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                     uint32_t port)
{
    (void)ctx;
    struct portalloc_portalloc_data *d = pa(obj);
    for (size_t i = 0; i < PORTALLOC_MAX_ENTRIES; ++i) {
        if (d->entries[i].used && d->entries[i].port == port) {
            d->entries[i].used = 0;
            d->count--;
            yinfo("portalloc: released port %u", port);
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_OK(picomesh_int, 0);
}

PICOMESH_CLASS_ANNOTATE("override@portalloc:portalloc:portalloc_count_used")
struct picomesh_size_result portalloc_portalloc_count_used_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return PICOMESH_OK(picomesh_size, pa(obj)->count);
}

/* List ALL port allocations this service manages, as a JSON array
 * `[{"service_id":…,"port":…}, …]` (gh#15) — every allocation, not a count.
 * State is the in-memory entry table. */
/* Build the allocation list as JSON, honoring offset/limit (< 0 == all). */
static struct picomesh_json_result portalloc_list_window(struct object *obj, int64_t offset, int64_t limit)
{
    struct portalloc_portalloc_data *d = pa(obj);
    struct yjson_writer *w = yjson_writer_new();
    if (!w) return PICOMESH_ERR(picomesh_json, "portalloc_list: writer alloc failed");
    yjson_writer_begin_array(w);
    int64_t skip = offset > 0 ? offset : 0, emitted = 0;
    for (size_t i = 0; i < PORTALLOC_MAX_ENTRIES && (limit < 0 || emitted < limit); ++i) {
        if (!d->entries[i].used) continue;
        if (skip > 0) { --skip; continue; }
        yjson_writer_begin_object(w);
        yjson_writer_key(w, "service_id"); yjson_writer_int(w, (int64_t)d->entries[i].service_id);
        yjson_writer_key(w, "port");       yjson_writer_int(w, (int64_t)d->entries[i].port);
        yjson_writer_end_object(w);
        ++emitted;
    }
    yjson_writer_end_array(w);
    size_t len = 0;
    const char *data = yjson_writer_data(w, &len);
    char *out = strdup(data ? data : "[]");
    yjson_writer_free(w);
    if (!out) return PICOMESH_ERR(picomesh_json, "portalloc_list: strdup failed");
    return PICOMESH_OK(picomesh_json, out);
}

/* List ALL port allocations as a JSON array `[{"service_id":…,"port":…}]`,
 * paginated by offset/limit (gh#15). */
PICOMESH_CLASS_ANNOTATE("override@portalloc:portalloc:portalloc_list")
struct picomesh_json_result portalloc_portalloc_list_impl(struct ctx *ctx, struct object *obj,
                                                      struct yheaders *hdrs,
                                                      int64_t offset, int64_t limit)
{
    (void)ctx; (void)hdrs;
    if (limit <= 0) limit = 100;
    return portalloc_list_window(obj, offset, limit);
}

/* Unbounded variant — every allocation. */
PICOMESH_CLASS_ANNOTATE("override@portalloc:portalloc:portalloc_list_all")
struct picomesh_json_result portalloc_portalloc_list_all_impl(struct ctx *ctx, struct object *obj,
                                                              struct yheaders *hdrs)
{
    (void)ctx; (void)hdrs;
    return portalloc_list_window(obj, 0, -1);
}

#include "portalloc.gen.c"
