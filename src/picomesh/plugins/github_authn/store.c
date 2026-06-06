/* github_authn — GitHub OAuth App bridge.
 *
 * Picoforge's GitHub login follows Woodpecker's forge model: the user
 * authorizes a GitHub OAuth App, the gateway exchanges the one-time code
 * for a GitHub OAuth access token, and the token stays server-side. The
 * browser receives only the ordinary picomesh opaque session cookie.
 *
 *   exchange_code(code, redirect_uri)
 *       -> {"uid":..., "username":"...", "github_id":...}
 *
 * Tokens are persisted in sharded_storage under the `github_authn`
 * context. The HTTPS calls to GitHub go through the vendored static libcurl
 * (OpenSSL TLS) — see build-tools/3rdparty/libcurl. Secrets (the OAuth
 * client_secret and the GitHub access token) are handed to libcurl as a POST
 * body / request header and stay in THIS process's memory: they never appear
 * on a child process's argv (a `ps` listing is world-visible to same-host
 * users) and no shell is ever spawned.
 *
 * The old register_code/resolve methods remain for existing tests and for
 * offline smoke flows. */

#define _POSIX_C_SOURCE 200809L

#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yclass/class.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/ycore/idkey.h>
#include <picomesh/yjson/yjson.h>
#include <picomesh/plugin/sharded_storage/sharded_storage.h>
#include <picomesh/plugin/accounts/accounts.h>

#include <curl/curl.h>

#include <ctype.h>
#include <stdio.h>
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

static const char *cfg_string(const char *path, const char *fallback)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e) return fallback;
    struct yconfig_node_ptr_result r = yconfig_get(picomesh_engine_config(e), path);
    const char *out = (PICOMESH_IS_OK(r) && r.value) ? yconfig_node_as_string(r.value, fallback) : fallback;
    if (PICOMESH_IS_ERR(r)) picomesh_error_destroy(r.error);
    return out && *out ? out : fallback;
}

/* Growable response sink for a libcurl transfer. */
struct gh_http_buf {
    char  *data;
    size_t len;
};

/* CURLOPT_WRITEFUNCTION sink: append the body chunk, NUL-terminating as we go,
 * with a hard ceiling so a hostile/huge response can't exhaust memory. */
static size_t gh_http_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct gh_http_buf *buf = userdata;
    size_t add = size * nmemb;
    if (buf->len + add + 1 < buf->len) return 0;          /* size_t wrap */
    if (buf->len + add > 4u * 1024 * 1024) return 0;      /* 4 MiB ceiling */
    char *grown = realloc(buf->data, buf->len + add + 1);
    if (!grown) return 0;
    buf->data = grown;
    memcpy(buf->data + buf->len, ptr, add);
    buf->len += add;
    buf->data[buf->len] = 0;
    return add;
}

/* Common libcurl options for the GitHub calls (timeouts, no signals so it is
 * safe on the engine's worker threads, no redirects). */
static void gh_http_setup(CURL *curl, struct gh_http_buf *buf)
{
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "picoforge");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gh_http_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
}

/* 1 if the transfer succeeded with a 2xx status. */
static int gh_http_ok(CURL *curl, CURLcode rc)
{
    if (rc != CURLE_OK) return 0;
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return status >= 200 && status < 300;
}

/* Exchange the one-time OAuth code for a GitHub access token. The client_secret
 * is set as CURLOPT_POSTFIELDS — it lives only in this process's heap, never on
 * any argv. Returns the token (caller frees) or NULL. */
static char *github_token_exchange(const char *code, const char *redirect_uri)
{
    const char *client = cfg_string("github_authn.client_id", getenv("PICOFORGE_GITHUB_CLIENT"));
    const char *secret = cfg_string("github_authn.client_secret", getenv("PICOFORGE_GITHUB_SECRET"));
    const char *url = cfg_string("github_authn.oauth_url", "https://github.com/login/oauth/access_token");
    if (!client || !*client || !secret || !*secret) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char *enc_client   = curl_easy_escape(curl, client, 0);
    char *enc_secret   = curl_easy_escape(curl, secret, 0);
    char *enc_code     = curl_easy_escape(curl, code, 0);
    char *enc_redirect = curl_easy_escape(curl, redirect_uri, 0);
    struct curl_slist *headers = NULL;
    struct gh_http_buf buf = {0};
    char *body = NULL, *token = NULL;

    if (!enc_client || !enc_secret || !enc_code || !enc_redirect) goto done;
    size_t need = strlen(enc_client) + strlen(enc_secret) +
                  strlen(enc_code) + strlen(enc_redirect) + 64;
    body = malloc(need);
    if (!body) goto done;
    snprintf(body, need, "client_id=%s&client_secret=%s&code=%s&redirect_uri=%s",
             enc_client, enc_secret, enc_code, enc_redirect);

    headers = curl_slist_append(NULL, "Accept: application/json");
    if (!headers) goto done;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    gh_http_setup(curl, &buf);

    CURLcode rc = curl_easy_perform(curl);
    if (gh_http_ok(curl, rc) && buf.data) {
        struct yjson_doc *doc = yjson_parse(buf.data, strlen(buf.data));
        if (doc) {
            const char *tok = yjson_as_string(
                yjson_object_get(yjson_doc_root(doc), "access_token"), NULL);
            if (tok && *tok) token = strdup(tok);
            yjson_doc_free(doc);
        }
    }
done:
    free(body);
    free(buf.data);
    curl_slist_free_all(headers);
    curl_free(enc_client);
    curl_free(enc_secret);
    curl_free(enc_code);
    curl_free(enc_redirect);
    curl_easy_cleanup(curl);
    return token;
}

/* Fetch the authenticated user's GitHub profile JSON. The access token is set
 * as an Authorization header — in-process only, never on any argv. Returns the
 * response body (caller frees) or NULL. */
static char *github_user_json(const char *access_token)
{
    const char *api = cfg_string("github_authn.api_url", "https://api.github.com");
    if (!access_token || !*access_token) return NULL;
    char api_user[768];
    int un = snprintf(api_user, sizeof(api_user), "%s/user", api ? api : "");
    if (un <= 0 || (size_t)un >= sizeof(api_user)) return NULL;

    char auth[2200];
    int an = snprintf(auth, sizeof(auth), "Authorization: Bearer %s", access_token);
    if (an <= 0 || (size_t)an >= sizeof(auth)) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    struct curl_slist *headers = NULL;
    struct gh_http_buf buf = {0};
    char *out = NULL;

    headers = curl_slist_append(NULL, "Accept: application/vnd.github+json");
    struct curl_slist *with_auth = headers ? curl_slist_append(headers, auth) : NULL;
    if (!with_auth) goto done;
    headers = with_auth;

    curl_easy_setopt(curl, CURLOPT_URL, api_user);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    gh_http_setup(curl, &buf);

    CURLcode rc = curl_easy_perform(curl);
    if (gh_http_ok(curl, rc) && buf.data) {
        out = buf.data;
        buf.data = NULL;
    }
done:
    free(buf.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return out;
}

static void normalize_github_login(const char *login, char *out, size_t cap)
{
    size_t o = 0;
    for (const char *p = login ? login : ""; *p && o + 1 < cap && o < 32; ++p) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') out[o++] = c;
    }
    out[o] = 0;
}

static void store_mapping(struct yheaders *hdrs, uint32_t uid, int64_t github_id, const char *username, const char *access_token)
{
    struct picomesh_engine *e = picomesh_active_engine();
    if (!e || !access_token || !*access_token || github_id <= 0) return;
    struct ctx c = picomesh_engine_service_ctx(e, "sharded_storage");
    struct object_ptr_result o = sharded_storage_db_create(&c);
    if (PICOMESH_IS_ERR(o)) { picomesh_error_destroy(o.error); return; }
    char key[96], val[512];
    snprintf(key, sizeof(key), "github:%lld:token", (long long)github_id);
    sharded_storage_db_set(&c, o.value, hdrs, "github_authn", key, access_token);
    snprintf(key, sizeof(key), "github:%lld:uid", (long long)github_id);
    snprintf(val, sizeof(val), "%u", uid);
    sharded_storage_db_set(&c, o.value, hdrs, "github_authn", key, val);
    snprintf(key, sizeof(key), "uid:%u:github_id", uid);
    snprintf(val, sizeof(val), "%lld", (long long)github_id);
    sharded_storage_db_set(&c, o.value, hdrs, "github_authn", key, val);
    snprintf(key, sizeof(key), "uid:%u:username", uid);
    sharded_storage_db_set(&c, o.value, hdrs, "github_authn", key, username ? username : "");
    object_release_in_ctx(&c, o.value);
}

PICOMESH_CLASS_ANNOTATE("override@github_authn:github_authn:github_authn_exchange_code")
struct picomesh_json_result github_authn_github_authn_exchange_code_impl(struct ctx *ctx, struct object *obj, struct yheaders *hdrs, const char *code, const char *redirect_uri)
{
    (void)ctx; (void)obj;
    if (!code || !*code || !redirect_uri || !*redirect_uri)
        return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: missing code or redirect_uri");
    char *token = github_token_exchange(code, redirect_uri);
    if (!token) return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: token exchange failed");
    char *user_json = github_user_json(token);
    if (!user_json) { free(token); return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: /user request failed"); }
    struct yjson_doc *doc = yjson_parse(user_json, strlen(user_json));
    free(user_json);
    if (!doc) { free(token); return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: bad /user JSON"); }
    const struct yjson_value *root = yjson_doc_root(doc);
    int64_t github_id = yjson_as_int(yjson_object_get(root, "id"), 0);
    const char *login = yjson_as_string(yjson_object_get(root, "login"), NULL);
    char username[64];
    normalize_github_login(login, username, sizeof(username));
    yjson_doc_free(doc);
    if (github_id <= 0 || !username[0]) { free(token); return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: missing GitHub id/login"); }
    uint32_t uid = picomesh_fnv1a32(username);
    if (!uid) uid = 1;
    store_mapping(hdrs, uid, github_id, username, token);
    free(token);
    struct yjson_writer *w = yjson_writer_new();
    if (!w) return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: writer alloc failed");
    yjson_writer_begin_object(w);
    yjson_writer_key(w, "uid");       yjson_writer_int(w, (int64_t)uid);
    yjson_writer_key(w, "username");  yjson_writer_string(w, username);
    yjson_writer_key(w, "github_id"); yjson_writer_int(w, github_id);
    yjson_writer_end_object(w);
    size_t len = 0;
    const char *data = yjson_writer_data(w, &len);
    char *out = data ? strdup(data) : NULL;
    yjson_writer_free(w);
    if (!out) return PICOMESH_ERR(picomesh_json, "github_authn_exchange_code: strdup failed");
    yinfo("github_authn: exchanged GitHub OAuth code for user=%s id=%lld", username, (long long)github_id);
    return PICOMESH_OK(picomesh_json, out);
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
