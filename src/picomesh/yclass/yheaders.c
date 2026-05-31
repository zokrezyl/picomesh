/* yheaders — generic request-header bag (see yheaders.h). */

#include <picomesh/yclass/yheaders.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yheaders_pair {
    char *key;
    char *value;
};

struct yheaders {
    struct yheaders_pair *items;
    size_t len;
    size_t cap;
};

struct yheaders *yheaders_new(void)
{
    return calloc(1, sizeof(struct yheaders));
}

void yheaders_free(struct yheaders *h)
{
    if (!h) return;
    for (size_t i = 0; i < h->len; ++i) {
        free(h->items[i].key);
        free(h->items[i].value);
    }
    free(h->items);
    free(h);
}

static struct yheaders_pair *find(const struct yheaders *h, const char *key)
{
    if (!h || !key) return NULL;
    for (size_t i = 0; i < h->len; ++i)
        if (strcmp(h->items[i].key, key) == 0)
            return &h->items[i];
    return NULL;
}

int yheaders_set(struct yheaders *h, const char *key, const char *value)
{
    if (!h || !key) return -1;
    if (!value) value = "";

    struct yheaders_pair *p = find(h, key);
    if (p) {
        char *nv = strdup(value);
        if (!nv) return -1;
        free(p->value);
        p->value = nv;
        return 0;
    }

    if (h->len == h->cap) {
        size_t nc = h->cap ? h->cap * 2 : 8;
        struct yheaders_pair *ni = realloc(h->items, nc * sizeof(*ni));
        if (!ni) return -1;
        h->items = ni;
        h->cap = nc;
    }
    char *k = strdup(key);
    char *v = strdup(value);
    if (!k || !v) { free(k); free(v); return -1; }
    h->items[h->len].key = k;
    h->items[h->len].value = v;
    h->len++;
    return 0;
}

const char *yheaders_get(const struct yheaders *h, const char *key)
{
    struct yheaders_pair *p = find(h, key);
    return p ? p->value : NULL;
}

int yheaders_set_u32(struct yheaders *h, const char *key, uint32_t v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%" PRIu32, v);
    return yheaders_set(h, key, buf);
}

uint32_t yheaders_get_u32(const struct yheaders *h, const char *key, uint32_t fallback)
{
    const char *v = yheaders_get(h, key);
    if (!v || !*v) return fallback;
    char *end = NULL;
    unsigned long n = strtoul(v, &end, 10);
    return (end && *end == 0) ? (uint32_t)n : fallback;
}

int yheaders_set_u64(struct yheaders *h, const char *key, uint64_t v)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%" PRIu64, v);
    return yheaders_set(h, key, buf);
}

uint64_t yheaders_get_u64(const struct yheaders *h, const char *key, uint64_t fallback)
{
    const char *v = yheaders_get(h, key);
    if (!v || !*v) return fallback;
    char *end = NULL;
    unsigned long long n = strtoull(v, &end, 10);
    return (end && *end == 0) ? (uint64_t)n : fallback;
}

size_t yheaders_count(const struct yheaders *h)
{
    return h ? h->len : 0;
}

void yheaders_for_each(const struct yheaders *h,
                       void (*cb)(const char *key, const char *value, void *ud),
                       void *ud)
{
    if (!h || !cb) return;
    for (size_t i = 0; i < h->len; ++i)
        cb(h->items[i].key, h->items[i].value, ud);
}

struct yheaders *yheaders_copy(const struct yheaders *h)
{
    struct yheaders *out = yheaders_new();
    if (!out || !h) return out;
    for (size_t i = 0; i < h->len; ++i) {
        if (yheaders_set(out, h->items[i].key, h->items[i].value) != 0) {
            yheaders_free(out);
            return NULL;
        }
    }
    return out;
}

/* ---- wire (de)serialization --------------------------------------- */
/* u16 count, then per pair: u16 klen, key, u32 vlen, val. */

size_t yheaders_serialized_size(const struct yheaders *h)
{
    size_t n = 2; /* count */
    if (!h) return n;
    for (size_t i = 0; i < h->len; ++i) {
        n += 2 + strlen(h->items[i].key);
        n += 4 + strlen(h->items[i].value);
    }
    return n;
}

size_t yheaders_serialize(const struct yheaders *h, void *buf, size_t cap)
{
    size_t need = yheaders_serialized_size(h);
    if (cap < need) return 0;

    uint8_t *p = buf;
    size_t off = 0;
    uint16_t count = h ? (uint16_t)h->len : 0;
    memcpy(p + off, &count, 2); off += 2;
    if (!h) return off;

    for (size_t i = 0; i < h->len; ++i) {
        uint16_t klen = (uint16_t)strlen(h->items[i].key);
        uint32_t vlen = (uint32_t)strlen(h->items[i].value);
        memcpy(p + off, &klen, 2); off += 2;
        memcpy(p + off, h->items[i].key, klen); off += klen;
        memcpy(p + off, &vlen, 4); off += 4;
        memcpy(p + off, h->items[i].value, vlen); off += vlen;
    }
    return off;
}

struct yheaders *yheaders_parse(const void *buf, size_t len, size_t *consumed)
{
    const uint8_t *p = buf;
    size_t off = 0;
    if (len < 2) return NULL;
    uint16_t count;
    memcpy(&count, p + off, 2); off += 2;

    struct yheaders *h = yheaders_new();
    if (!h) return NULL;

    for (uint16_t i = 0; i < count; ++i) {
        if (off + 2 > len) goto bad;
        uint16_t klen;
        memcpy(&klen, p + off, 2); off += 2;
        if (off + klen > len) goto bad;
        char *key = malloc((size_t)klen + 1);
        if (!key) goto bad;
        memcpy(key, p + off, klen); key[klen] = 0; off += klen;

        if (off + 4 > len) { free(key); goto bad; }
        uint32_t vlen;
        memcpy(&vlen, p + off, 4); off += 4;
        if (off + vlen > len) { free(key); goto bad; }
        char *val = malloc((size_t)vlen + 1);
        if (!val) { free(key); goto bad; }
        memcpy(val, p + off, vlen); val[vlen] = 0; off += vlen;

        int rc = yheaders_set(h, key, val);
        free(key);
        free(val);
        if (rc != 0) goto bad;
    }

    if (consumed) *consumed = off;
    return h;
bad:
    yheaders_free(h);
    return NULL;
}
