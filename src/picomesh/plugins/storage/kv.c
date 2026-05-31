/* Storage example — a single class with set/get/count methods.
 *
 * Mirrors yaapp's `examples/plugins/storage` (key-value store, exposed
 * via a few methods). All public-stub plumbing + the RPC skels are
 * generated from these annotations; the file you're reading is the
 * only thing a user writes. */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KV_MAX_ENTRIES 64
#define KV_KEY_MAX     64
#define KV_VALUE_MAX   256

struct entry {
    char key[KV_KEY_MAX];
    char value[KV_VALUE_MAX]; /* opaque string/byte value (NUL-terminated) */
    int used;
};

struct PICOMESH_CLASS_ANNOTATE("class@storage:kv") storage_kv_data {
    struct entry entries[KV_MAX_ENTRIES];
    size_t count;
};

static struct storage_kv_data *kv_data(struct object *obj)
{
    return (struct storage_kv_data *)((char *)obj + sizeof(struct object));
}

/* Copy an opaque string value into a fixed entry slot (NUL-terminated,
 * truncated at KV_VALUE_MAX-1). */
static void kv_store_value(struct entry *e, const char *value)
{
    size_t n = strlen(value);
    if (n >= KV_VALUE_MAX) n = KV_VALUE_MAX - 1;
    memcpy(e->value, value, n);
    e->value[n] = 0;
}

PICOMESH_CLASS_ANNOTATE("override@storage:kv:kv_set")
struct picomesh_int_result storage_kv_set_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                             const char *key, const char *value)
{
    (void)ctx; (void)hdrs;
    if (!key || !*key) return PICOMESH_ERR(picomesh_int, "kv_set: empty key");
    if (!value) value = "";
    struct storage_kv_data *d = kv_data(obj);

    for (size_t i = 0; i < KV_MAX_ENTRIES; ++i) {
        if (d->entries[i].used && strcmp(d->entries[i].key, key) == 0) {
            kv_store_value(&d->entries[i], value);
            yinfo("kv_set: updated %s=%s", key, d->entries[i].value);
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    for (size_t i = 0; i < KV_MAX_ENTRIES; ++i) {
        if (!d->entries[i].used) {
            strncpy(d->entries[i].key, key, KV_KEY_MAX - 1);
            d->entries[i].key[KV_KEY_MAX - 1] = 0;
            kv_store_value(&d->entries[i], value);
            d->entries[i].used = 1;
            d->count++;
            yinfo("kv_set: inserted %s=%s", key, d->entries[i].value);
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_ERR(picomesh_int, "kv_set: store full");
}

PICOMESH_CLASS_ANNOTATE("override@storage:kv:kv_get")
struct picomesh_string_result storage_kv_get_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                               const char *key)
{
    (void)ctx; (void)hdrs;
    if (!key || !*key) return PICOMESH_ERR(picomesh_string, "kv_get: empty key");
    struct storage_kv_data *d = kv_data(obj);

    for (size_t i = 0; i < KV_MAX_ENTRIES; ++i) {
        if (d->entries[i].used && strcmp(d->entries[i].key, key) == 0) {
            char *out = strdup(d->entries[i].value);
            if (!out) return PICOMESH_ERR(picomesh_string, "kv_get: out of memory");
            yinfo("kv_get: %s -> %s", key, out);
            return PICOMESH_OK(picomesh_string, out);
        }
    }
    return PICOMESH_ERR(picomesh_string, "kv_get: key not found");
}

PICOMESH_CLASS_ANNOTATE("override@storage:kv:kv_count")
struct picomesh_size_result storage_kv_count_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    struct storage_kv_data *d = kv_data(obj);
    yinfo("kv_count: %zu entries", d->count);
    return PICOMESH_OK(picomesh_size, d->count);
}

/* Pull in the generated accessor — emitted by src/picomesh/yclass/gen/codegen.py and
 * compiled in-line so the static class descriptor table lives in this
 * translation unit. */
#include "kv.gen.c"
