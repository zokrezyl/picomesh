/* yhttp — HTTP/1.1 + JSON frontend.
 *
 * Per-peer serve coroutine reads one HTTP request via picohttpparser,
 * routes it, writes the HTTP/1.1 response. Body must be Content-Length
 * delimited (no chunked yet); we cap bodies at 256 KB.
 *
 * Body parsing on the way in: simdjson (yjson_parse).
 * Body serialization on the way out: yjson_writer.
 * Dispatch backend: jinvoke_for + the rpc handle table — same as yttp,
 * so the same instance handles span both frontends in one process. */

#include <yaafc/frontends/yhttp/yhttp.h>
#include <yaafc/yengine/engine.h>
#include <yaafc/yloop/yloop.h>
#include <yaafc/yclass/class.h>
#include <yaafc/yclass/rpc.h>
#include <yaafc/yclass/jinvoke.h>
#include <yaafc/yconfig/yconfig.h>
#include <yaafc/yjson/yjson.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <picohttpparser.h>

#include "frontend.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YHTTP_REQ_BUF      (256 * 1024)
#define YHTTP_MAX_HEADERS  64

struct yhttp_frontend {
    struct yaafc_engine *engine;
    char *static_root; /* may be NULL */
};

/* ---- helpers ---------------------------------------------------- */

static int header_match(const struct phr_header *hdrs, size_t n,
                        const char *want, char *out, size_t out_cap)
{
    size_t wl = strlen(want);
    for (size_t i = 0; i < n; ++i) {
        if (hdrs[i].name_len != wl) continue;
        int ok = 1;
        for (size_t j = 0; j < wl; ++j) {
            char a = hdrs[i].name[j];
            char b = want[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { ok = 0; break; }
        }
        if (!ok) continue;
        size_t copy = hdrs[i].value_len < out_cap - 1
                          ? hdrs[i].value_len : out_cap - 1;
        memcpy(out, hdrs[i].value, copy);
        out[copy] = 0;
        return 1;
    }
    return 0;
}

static void send_response(struct yloop_stream *s, int status,
                          const char *content_type,
                          const char *body, size_t body_len, int keep_alive)
{
    const char *reason = "OK";
    if (status == 400) reason = "Bad Request";
    else if (status == 404) reason = "Not Found";
    else if (status == 405) reason = "Method Not Allowed";
    else if (status == 500) reason = "Internal Server Error";

    /* CORS: pages served by the frontend at :8080 fetch JSON from
     * other services on :8201..:8211 — cross-origin from the browser's
     * perspective. Always-on `*` keeps the local demo simple; a real
     * deploy would gate this on a configured origins list. */
    char header[768];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n",
        status, reason,
        content_type,
        body_len,
        keep_alive ? "keep-alive" : "close");
    if (n <= 0) return;
    yloop_write(s, header, (size_t)n);
    if (body_len) yloop_write(s, body, body_len);
}

static void send_json_error(struct yloop_stream *s, int status,
                            int code, const char *message, int keep_alive)
{
    struct yjson_writer *w = yjson_writer_new();
    yjson_w_begin_object(w);
    yjson_w_key(w, "error");
    yjson_w_begin_object(w);
    yjson_w_key(w, "code");    yjson_w_int(w, code);
    yjson_w_key(w, "message"); yjson_w_string(w, message);
    yjson_w_end_object(w);
    yjson_w_end_object(w);
    size_t len;
    const char *data = yjson_w_data(w, &len);
    send_response(s, status, "application/json", data, len, keep_alive);
    yjson_writer_free(w);
}

/* ---- route handlers --------------------------------------------- */

static void route_root(struct yloop_stream *s, int keep_alive)
{
    static const char body[] =
        "yaafc/yhttp frontend\n"
        "\n"
        "POST /create   {\"class\": \"<plugin>_<class>\"}                            -> {\"handle\": u64}\n"
        "POST /invoke   {\"method\": \"<qname>\", \"handle\": u64, \"args\": [...]}    -> {\"result\": ...}\n"
        "GET  /describe?class=NAME                                              -> {\"class\":..., \"methods\":[...]}\n";
    send_response(s, 200, "text/plain", body, sizeof(body) - 1, keep_alive);
}

/* ---- static file serving --------------------------------------- */

/* Map an extension → MIME. Tiny table; defaults to application/octet-stream. */
static const char *mime_for(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcmp(dot, ".html") || !strcmp(dot, ".htm")) return "text/html; charset=utf-8";
    if (!strcmp(dot, ".css"))  return "text/css; charset=utf-8";
    if (!strcmp(dot, ".js"))   return "application/javascript; charset=utf-8";
    if (!strcmp(dot, ".json")) return "application/json; charset=utf-8";
    if (!strcmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcmp(dot, ".png"))  return "image/png";
    if (!strcmp(dot, ".ico"))  return "image/x-icon";
    if (!strcmp(dot, ".txt"))  return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

/* Try to serve `<root>/<url_path>` as a static file. Returns 1 if it
 * sent a response, 0 if the URL doesn't resolve to a file (caller
 * falls through to its own 404). Path-traversal is blocked by
 * rejecting any `..` component before the join. */
static int serve_static(struct yloop_stream *s, const char *root,
                        const char *url_path, int keep_alive)
{
    if (!root || !*root) return 0;
    /* Trim leading slash — we always join `root + "/" + url_path`. */
    const char *rel = url_path;
    while (*rel == '/') ++rel;
    /* GET / maps to index.html. */
    char rel_buf[1024];
    if (!*rel) {
        snprintf(rel_buf, sizeof(rel_buf), "index.html");
        rel = rel_buf;
    }
    if (strstr(rel, "..")) return 0;

    char full[2048];
    int n = snprintf(full, sizeof(full), "%s/%s", root, rel);
    if (n <= 0 || n >= (int)sizeof(full)) return 0;

    /* Strip any query string from the file path before stat. */
    char *q = strchr(full, '?');
    if (q) *q = 0;

    struct stat st;
    if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) return 0;

    int fd = open(full, O_RDONLY);
    if (fd < 0) return 0;
    size_t cap = (size_t)st.st_size;
    char *buf = malloc(cap);
    if (!buf) { close(fd); return 0; }
    size_t got = 0;
    while (got < cap) {
        ssize_t r = read(fd, buf + got, cap - got);
        if (r <= 0) break;
        got += (size_t)r;
    }
    close(fd);
    if (got != cap) { free(buf); return 0; }

    send_response(s, 200, mime_for(full), buf, cap, keep_alive);
    free(buf);
    return 1;
}

static void route_create(struct yloop_stream *s, const char *body, size_t blen,
                         int keep_alive)
{
    struct yjson_doc *doc = yjson_parse(body, blen);
    if (!doc) { send_json_error(s, 400, -32700, "invalid JSON", keep_alive); return; }
    const struct yjson_value *root = yjson_doc_root(doc);
    const char *cls = yjson_as_string(yjson_object_get(root, "class"), NULL);
    if (!cls) {
        yjson_doc_free(doc);
        send_json_error(s, 400, -32602, "create: missing 'class'", keep_alive);
        return;
    }
    struct class_ptr_result cr = class_by_name(cls);
    if (YAAFC_IS_ERR(cr)) {
        yaafc_error_destroy(cr.error);
        char msg[256];
        snprintf(msg, sizeof(msg), "create: unknown class '%s'", cls);
        yjson_doc_free(doc);
        send_json_error(s, 404, -32601, msg, keep_alive);
        return;
    }
    struct object_ptr_result orr = object_alloc(cr.value);
    if (YAAFC_IS_ERR(orr)) {
        yaafc_error_destroy(orr.error);
        yjson_doc_free(doc);
        send_json_error(s, 500, -32603, "create: object_alloc failed", keep_alive);
        return;
    }
    uint64_t handle = rpc_register_object(orr.value);
    yjson_doc_free(doc);

    struct yjson_writer *w = yjson_writer_new();
    yjson_w_begin_object(w);
    yjson_w_key(w, "handle"); yjson_w_int(w, (int64_t)handle);
    yjson_w_end_object(w);
    size_t len; const char *data = yjson_w_data(w, &len);
    send_response(s, 200, "application/json", data, len, keep_alive);
    yjson_writer_free(w);
}

static void route_invoke(struct yloop_stream *s, const char *body, size_t blen,
                         int keep_alive)
{
    struct yjson_doc *doc = yjson_parse(body, blen);
    if (!doc) { send_json_error(s, 400, -32700, "invalid JSON", keep_alive); return; }
    const struct yjson_value *root = yjson_doc_root(doc);
    const char *method = yjson_as_string(yjson_object_get(root, "method"), NULL);
    int64_t handle = yjson_as_int(yjson_object_get(root, "handle"), 0);
    const struct yjson_value *args = yjson_object_get(root, "args");

    if (!method) { yjson_doc_free(doc); send_json_error(s, 400, -32602, "invoke: missing 'method'", keep_alive); return; }
    if (!handle) { yjson_doc_free(doc); send_json_error(s, 400, -32602, "invoke: missing/zero 'handle'", keep_alive); return; }

    void *obj = rpc_handle_resolve((uint64_t)handle);
    if (!obj) { yjson_doc_free(doc); send_json_error(s, 404, -32602, "invoke: unknown handle", keep_alive); return; }

    jinvoke_fn fn = jinvoke_for(method);
    if (!fn) {
        char msg[256]; snprintf(msg, sizeof(msg), "invoke: no jinvoke for '%s'", method);
        yjson_doc_free(doc);
        send_json_error(s, 404, -32601, msg, keep_alive);
        return;
    }

    struct yjson_writer *w = yjson_writer_new();
    yjson_w_begin_object(w);
    yjson_w_key(w, "result");
    char err[256] = {0};
    int rc = fn((struct object *)obj, args, w, err, sizeof(err));
    if (rc != 0) {
        yjson_writer_free(w);
        yjson_doc_free(doc);
        send_json_error(s, 500, -32000, err[0] ? err : "invoke: impl failed", keep_alive);
        return;
    }
    yjson_w_end_object(w);
    size_t len; const char *data = yjson_w_data(w, &len);
    send_response(s, 200, "application/json", data, len, keep_alive);
    yjson_writer_free(w);
    yjson_doc_free(doc);
}

/* Forward-decl for the URL ?key=val extractor; the real definition
 * sits a few hundred lines below alongside other URL helpers. */
static const char *query_get(const char *path, const char *key,
                             char *out, size_t out_cap);

/* Parse one HTTP header value out of the raw header block into `out`.
 * Returns 1 on hit. Used by the /_rpc handler to read auth context
 * out of `X-Yaafc-Uid` / `X-Yaafc-Sid`. */
static int header_get(const char *raw, size_t raw_len, const char *name,
                      char *out, size_t out_cap)
{
    size_t nlen = strlen(name);
    const char *p = raw;
    const char *end = raw + raw_len;
    while (p < end) {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        if (!eol) break;
        size_t llen = (size_t)(eol - p);
        if (llen && p[llen - 1] == '\r') llen--;
        if (llen > nlen + 1 && p[nlen] == ':' &&
            strncasecmp(p, name, nlen) == 0) {
            const char *v = p + nlen + 1;
            while (v < p + llen && (*v == ' ' || *v == '\t')) ++v;
            size_t vl = (size_t)(p + llen - v);
            if (vl >= out_cap) vl = out_cap - 1;
            memcpy(out, v, vl);
            out[vl] = 0;
            return 1;
        }
        p = eol + 1;
    }
    return 0;
}

/* POST /_rpc?op=<op>&id=<id> — binary RPC over HTTP. The body is the
 * same packed-args payload the legacy yrpc protocol carried; we just
 * wrap it in HTTP so auth and (later) tracing context can travel as
 * headers. Mirrors yaapp's `POST /_rpc` boundary at the gateway. */
static void route_rpc_binary(struct yloop_stream *s,
                             const char *path,
                             const char *headers_raw, size_t headers_raw_len,
                             const char *body, size_t body_len,
                             int keep_alive)
{
    char op_s[16] = {0}, id_s[16] = {0};
    query_get(path, "op",  op_s, sizeof(op_s));
    query_get(path, "id",  id_s, sizeof(id_s));
    if (!*op_s) { send_json_error(s, 400, -32602, "_rpc: missing op", keep_alive); return; }
    enum rpc_op op = (enum rpc_op)atoi(op_s);
    uint32_t id  = (uint32_t)strtoul(id_s, NULL, 10);

    /* Auth context flows in HTTP headers. Backend trusts the caller
     * (frontend) — same trust model as yaapp's gateway. */
    char uid_s[16] = {0}, sid_s[16] = {0};
    header_get(headers_raw, headers_raw_len, "X-Yaafc-Uid", uid_s, sizeof(uid_s));
    header_get(headers_raw, headers_raw_len, "X-Yaafc-Sid", sid_s, sizeof(sid_s));
    uint32_t caller_uid = (uint32_t)strtoul(uid_s, NULL, 10);
    uint32_t caller_sid = (uint32_t)strtoul(sid_s, NULL, 10);

    /* Stash the caller's ctx where the skel can pick it up.
     * Coroutines are cooperative and skel runs to completion (yields
     * to libuv only on its own outbound rpc_calls); ctx is read AT
     * the top of the skel before any yield can clobber it. */
    extern void rpc_set_current_caller(uint32_t uid, uint32_t sid);
    rpc_set_current_caller(caller_uid, caller_sid);

    uint8_t resp[8192];
    size_t resp_len = 0;
    switch (op) {
    case RPC_OP_CALL: {
        rpc_skel_fn fn = rpc_skel_for((method_slot)id);
        if (fn) resp_len = fn(body, body_len, resp, sizeof(resp));
        break;
    }
    case RPC_OP_RESOLVE_SLOT:
    case RPC_OP_GET_CLASS:
    case RPC_OP_CREATE:
    case RPC_OP_DESTROY:
        /* Dispatch the admin ops via the existing inline handlers.
         * They take (body, body_len, resp, resp_max) → bytes_written. */
        {
            extern size_t rpc_handle_admin_op(enum rpc_op op,
                const void *body, size_t body_len, void *resp, size_t resp_max);
            resp_len = rpc_handle_admin_op(op, body, body_len, resp, sizeof(resp));
        }
        break;
    }
    rpc_set_current_caller(0, 0);

    send_response(s, 200, "application/octet-stream", (const char *)resp, resp_len, keep_alive);
}

struct describe_ctx {
    struct yjson_writer *w;
};

static void describe_emit(const char *name, method_slot slot, void *ud)
{
    (void)slot;
    struct describe_ctx *dc = ud;
    yjson_w_string(dc->w, name);
}

/* Extract the value of ?class=… from a URL like /describe?class=foo. */
static const char *query_get(const char *path, const char *key,
                             char *out, size_t out_cap)
{
    const char *q = strchr(path, '?');
    if (!q) return NULL;
    size_t klen = strlen(key);
    const char *p = q + 1;
    while (*p) {
        const char *eq = strchr(p, '=');
        if (!eq) return NULL;
        if ((size_t)(eq - p) == klen && strncmp(p, key, klen) == 0) {
            const char *end = strchr(eq + 1, '&');
            size_t vl = end ? (size_t)(end - eq - 1) : strlen(eq + 1);
            if (vl >= out_cap) vl = out_cap - 1;
            memcpy(out, eq + 1, vl);
            out[vl] = 0;
            return out;
        }
        const char *nxt = strchr(p, '&');
        if (!nxt) break;
        p = nxt + 1;
    }
    return NULL;
}

static void route_describe(struct yloop_stream *s, const char *path,
                           int keep_alive)
{
    char cls[128];
    if (!query_get(path, "class", cls, sizeof(cls))) {
        send_json_error(s, 400, -32602, "describe: missing ?class=", keep_alive);
        return;
    }
    struct class_ptr_result cr = class_by_name(cls);
    if (YAAFC_IS_ERR(cr)) {
        yaafc_error_destroy(cr.error);
        char msg[256]; snprintf(msg, sizeof(msg), "describe: unknown class '%s'", cls);
        send_json_error(s, 404, -32601, msg, keep_alive);
        return;
    }
    struct yjson_writer *w = yjson_writer_new();
    yjson_w_begin_object(w);
    yjson_w_key(w, "class");   yjson_w_string(w, cls);
    yjson_w_key(w, "methods"); yjson_w_begin_array(w);
    struct describe_ctx dc = {.w = w};
    class_for_each_slot(cr.value, describe_emit, &dc);
    yjson_w_end_array(w);
    yjson_w_end_object(w);
    size_t len; const char *data = yjson_w_data(w, &len);
    send_response(s, 200, "application/json", data, len, keep_alive);
    yjson_writer_free(w);
}

/* ---- request loop ---------------------------------------------- */

static int starts_with(const char *s, size_t s_len, const char *prefix)
{
    size_t pl = strlen(prefix);
    return s_len >= pl && memcmp(s, prefix, pl) == 0;
}

static void serve_one(struct yloop *l, struct yloop_stream *s, void *ud)
{
    (void)l;
    struct yhttp_frontend *f = ud;
    const char *static_root = f ? f->static_root : NULL;
    yinfo("yhttp: peer connected");
    char *buf = malloc(YHTTP_REQ_BUF);
    if (!buf) { yloop_close(s); return; }

    for (;;) {
        size_t buf_len = 0;
        int phr_minor_version = 0;
        const char *phr_method = NULL;
        size_t phr_method_len = 0;
        const char *phr_path = NULL;
        size_t phr_path_len = 0;
        struct phr_header headers[YHTTP_MAX_HEADERS];
        size_t num_headers;
        int parsed = -2;

        /* phr_parse_request says "incomplete" with -2 — keep reading.
         * For simplicity we read up to YHTTP_REQ_BUF in 4-KB chunks. */
        while (parsed == -2) {
            if (buf_len >= YHTTP_REQ_BUF) goto close_peer;
            size_t chunk = YHTTP_REQ_BUF - buf_len;
            if (chunk > 4096) chunk = 4096;
            /* read_some: returns whatever bytes are available, not
             * exactly `chunk` — critical for HTTP request lines that
             * arrive in single packets shorter than our buffer. */
            size_t got = yloop_read_some(s, buf + buf_len, chunk);
            if (got == 0) goto close_peer;
            buf_len += got;
            num_headers = YHTTP_MAX_HEADERS;
            parsed = phr_parse_request(buf, buf_len,
                                       &phr_method, &phr_method_len,
                                       &phr_path, &phr_path_len,
                                       &phr_minor_version,
                                       headers, &num_headers, 0);
        }
        if (parsed < 0) goto close_peer;
        size_t header_end = (size_t)parsed;

        /* Pull headers we care about. */
        char clen_buf[32] = {0};
        char conn_buf[32] = {0};
        long content_length = 0;
        if (header_match(headers, num_headers, "Content-Length", clen_buf, sizeof(clen_buf))) {
            content_length = strtol(clen_buf, NULL, 10);
        }
        header_match(headers, num_headers, "Connection", conn_buf, sizeof(conn_buf));
        int keep_alive = strcasecmp(conn_buf, "keep-alive") == 0;

        /* Read the body if Content-Length > already-read tail. */
        if (content_length > 0) {
            size_t body_present = buf_len - header_end;
            while ((long)body_present < content_length) {
                if (header_end + content_length > YHTTP_REQ_BUF) goto close_peer;
                size_t need = (size_t)content_length - body_present;
                size_t got = yloop_read_some(s, buf + buf_len, need);
                if (got == 0) goto close_peer;
                buf_len += got;
                body_present += got;
            }
        }
        const char *body = buf + header_end;

        /* NUL-terminate the path so query parsing can use C-strings.
         * The path slice ends at phr_path + phr_path_len; the byte
         * right after is `HTTP/1.1\r\n` so overwriting it is safe
         * because we don't re-parse this request. */
        ((char *)phr_path)[phr_path_len] = 0;

        char method[16] = {0};
        size_t ml = phr_method_len < sizeof(method) - 1 ? phr_method_len : sizeof(method) - 1;
        memcpy(method, phr_method, ml);

        ydebug("yhttp: %s %s (body=%ld)", method, phr_path, content_length);

        /* Frontend page handlers get first crack at every request —
         * they pattern-match on URL, render HTML, and short-circuit if
         * they take ownership. Returns 0 → fall through to static /
         * JSON-API routes below. */
        if (yhttp_frontend_try(s, method, phr_path, buf, header_end,
                               body, (size_t)content_length, keep_alive)) {
            if (!keep_alive) goto close_peer;
            continue;
        }

        if (strcmp(method, "OPTIONS") == 0) {
            /* CORS preflight: browser sends OPTIONS before any
             * cross-origin POST with a JSON body. send_response()
             * already adds the Access-Control headers; an empty 200
             * is the right answer. */
            send_response(s, 200, "text/plain", "", 0, keep_alive);
        } else if (strcmp(method, "GET") == 0) {
            if (starts_with(phr_path, phr_path_len, "/describe")) {
                route_describe(s, phr_path, keep_alive);
            } else if (serve_static(s, static_root, phr_path, keep_alive)) {
                /* served from disk */
            } else if (strcmp(phr_path, "/") == 0) {
                /* fall-back help page when no static_root configured */
                route_root(s, keep_alive);
            } else {
                send_json_error(s, 404, -32601, "no such GET route", keep_alive);
            }
        } else if (strcmp(method, "POST") == 0) {
            if (strcmp(phr_path, "/create") == 0) {
                route_create(s, body, (size_t)content_length, keep_alive);
            } else if (strcmp(phr_path, "/invoke") == 0) {
                route_invoke(s, body, (size_t)content_length, keep_alive);
            } else if (strncmp(phr_path, "/_rpc", 5) == 0 &&
                       (phr_path[5] == 0 || phr_path[5] == '?')) {
                route_rpc_binary(s, phr_path, buf, header_end,
                                 body, (size_t)content_length, keep_alive);
            } else {
                send_json_error(s, 404, -32601, "no such POST route", keep_alive);
            }
        } else {
            send_json_error(s, 405, -32601, "method not allowed", keep_alive);
        }

        if (!keep_alive) goto close_peer;
    }
close_peer:
    free(buf);
    yinfo("yhttp: peer disconnected");
    yloop_close(s);
}

struct yhttp_frontend_ptr_result yhttp_start(struct yaafc_engine *e,
                                             const struct yhttp_config *cfg)
{
    if (!e) return YAAFC_ERR(yhttp_frontend_ptr, "yhttp_start: NULL engine");
    const char *host = (cfg && cfg->host) ? cfg->host : "127.0.0.1";
    int port = (cfg && cfg->port > 0) ? cfg->port : 8080;

    struct yloop *l = yaafc_engine_loop(e);
    if (!l) return YAAFC_ERR(yhttp_frontend_ptr, "yhttp_start: engine has no loop");

    struct yhttp_frontend *f = calloc(1, sizeof(*f));
    if (!f) return YAAFC_ERR(yhttp_frontend_ptr, "yhttp_start: calloc failed");
    f->engine = e;

    /* Pick up a static-file root from yconfig (`yhttp.static_dir`).
     * Each request resolves URL paths against this dir; unmatched URLs
     * fall through to the JSON API routes. */
    struct yconfig_node_ptr_result sr =
        yconfig_get(yaafc_engine_config(e), "yhttp.static_dir");
    if (YAAFC_IS_OK(sr) && sr.value) {
        const char *p = yconfig_node_as_string(sr.value, NULL);
        if (p && *p) {
            f->static_root = strdup(p);
            yinfo("yhttp: serving static files from '%s'", p);
        }
    }

    struct yaafc_void_result lr = yloop_listen_tcp(l, host, port, serve_one, f);
    if (YAAFC_IS_ERR(lr)) {
        free(f->static_root);
        free(f);
        return YAAFC_ERR(yhttp_frontend_ptr, "yhttp_start: yloop_listen_tcp failed", lr);
    }
    yinfo("yhttp: listening on %s:%d", host, port);
    return YAAFC_OK(yhttp_frontend_ptr, f);
}

void yhttp_stop(struct yhttp_frontend *f)
{
    if (!f) return;
    free(f->static_root);
    free(f);
}
