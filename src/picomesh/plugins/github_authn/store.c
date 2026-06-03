/* github_authn — GitHub OAuth bridge.
 *
 * The full plugin trades an authorization `code` for a GitHub user
 * identity and stitches it back into the accounts plugin. We don't
 * speak HTTPS here (no libcurl yet), so the methods exposed are
 * minimum viable shape:
 *
 *   set_credentials(client_id, secret_id)  → 1
 *   register_code(code, user_id)           → 1
 *   resolve(code)                          → user_id (0 if absent)
 *   count_codes                            → size
 *
 * In production `register_code` would happen as a side effect of the
 * upstream OAuth callback; `resolve` is what the login flow uses to
 * trade the code for an internal user_id. */

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yjson/yjson.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GH_MAX_CODES 128

struct gh_code_entry {
    uint32_t code;
    uint32_t user_id;
    int used;
};

struct PICOMESH_CLASS_ANNOTATE("class@github_authn:github_authn") github_authn_github_authn_data {
    uint32_t client_id;
    uint32_t secret_id;     /* opaque secret token id from yconfig substitution */
    struct gh_code_entry codes[GH_MAX_CODES];
    size_t count;
};

static struct github_authn_github_authn_data *gh(struct object *obj)
{
    return (struct github_authn_github_authn_data *)((char *)obj + sizeof(struct object));
}

PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_set_credentials")
struct picomesh_int_result github_authn_github_authn_set_credentials_impl(struct ctx *ctx,
                                                                struct object *obj,
                                                                struct yheaders *hdrs,
                                                                uint32_t client_id,
                                                                uint32_t secret_id)
{
    (void)ctx;
    struct github_authn_github_authn_data *d = gh(obj);
    d->client_id = client_id;
    d->secret_id = secret_id;
    yinfo("github_authn: credentials set (client_id=%u)", client_id);
    return PICOMESH_OK(picomesh_int, 1);
}

PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_register_code")
struct picomesh_int_result github_authn_github_authn_register_code_impl(struct ctx *ctx,
                                                              struct object *obj,
                                                              struct yheaders *hdrs,
                                                              uint32_t code, uint32_t user_id)
{
    (void)ctx;
    struct github_authn_github_authn_data *d = gh(obj);
    for (size_t i = 0; i < GH_MAX_CODES; ++i) {
        if (!d->codes[i].used) {
            d->codes[i].code = code;
            d->codes[i].user_id = user_id;
            d->codes[i].used = 1;
            d->count++;
            return PICOMESH_OK(picomesh_int, 1);
        }
    }
    return PICOMESH_ERR(picomesh_int, "github_authn_register_code: table full");
}

PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_resolve")
struct picomesh_uint32_result github_authn_github_authn_resolve_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                                                           uint32_t code)
{
    (void)ctx;
    struct github_authn_github_authn_data *d = gh(obj);
    for (size_t i = 0; i < GH_MAX_CODES; ++i) {
        if (d->codes[i].used && d->codes[i].code == code) {
            return PICOMESH_OK(picomesh_uint32, d->codes[i].user_id);
        }
    }
    return PICOMESH_OK(picomesh_uint32, 0);
}

PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_count_codes")
struct picomesh_size_result github_authn_github_authn_count_codes_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs)
{
    (void)ctx;
    return PICOMESH_OK(picomesh_size, gh(obj)->count);
}

/* List ALL OAuth codes this service manages, as a JSON array
 * `[{"code":…,"user_id":…}, …]` (gh#15) — every code, not a count. State is
 * the in-memory code table. */
/* Build the code list as JSON, honoring offset/limit (< 0 == all). */
static struct picomesh_json_result github_authn_list_window(struct object *obj, int64_t offset, int64_t limit)
{
    struct github_authn_github_authn_data *d = gh(obj);
    struct yjson_writer *w = yjson_writer_new();
    if (!w) return PICOMESH_ERR(picomesh_json, "github_authn_list: writer alloc failed");
    yjson_writer_begin_array(w);
    int64_t skip = offset > 0 ? offset : 0, emitted = 0;
    for (size_t i = 0; i < GH_MAX_CODES && (limit < 0 || emitted < limit); ++i) {
        if (!d->codes[i].used) continue;
        if (skip > 0) { --skip; continue; }
        yjson_writer_begin_object(w);
        yjson_writer_key(w, "code");    yjson_writer_int(w, (int64_t)d->codes[i].code);
        yjson_writer_key(w, "user_id"); yjson_writer_int(w, (int64_t)d->codes[i].user_id);
        yjson_writer_end_object(w);
        ++emitted;
    }
    yjson_writer_end_array(w);
    size_t len = 0;
    const char *data = yjson_writer_data(w, &len);
    char *out = strdup(data ? data : "[]");
    yjson_writer_free(w);
    if (!out) return PICOMESH_ERR(picomesh_json, "github_authn_list: strdup failed");
    return PICOMESH_OK(picomesh_json, out);
}

/* List ALL OAuth codes as a JSON array, paginated (gh#15). */
PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_list")
struct picomesh_json_result github_authn_github_authn_list_impl(struct ctx *ctx, struct object *obj,
                                                         struct yheaders *hdrs,
                                                         int64_t offset, int64_t limit)
{
    (void)ctx; (void)hdrs;
    if (limit <= 0) limit = 100;
    return github_authn_list_window(obj, offset, limit);
}

/* Unbounded variant — every code. */
PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_list_all")
struct picomesh_json_result github_authn_github_authn_list_all_impl(struct ctx *ctx, struct object *obj,
                                                                    struct yheaders *hdrs)
{
    (void)ctx; (void)hdrs;
    return github_authn_list_window(obj, 0, -1);
}

#include "store.gen.c"
