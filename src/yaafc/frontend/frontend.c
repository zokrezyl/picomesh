/* HTTP listener + router for the standalone yaafc-frontend binary.
 *
 * Per-peer serve coroutine reads one HTTP request via picohttpparser,
 * routes by method+path, writes the response. Modeled on yhttp.c but
 * carries no yaafc-engine state — all backend calls go out to the
 * gateway via the http client (see http_client.c).
 *
 * For now this file implements:
 *   GET  /login       hardcoded login page (no template renderer yet)
 *   GET  /static/...  serve from --static dir
 *   GET  /            redirect to /login
 *
 * Routes that need backend calls come as the HTTP client lands. */

#include "frontend.h"
#include "http_client.h"

#include <yaafc/ycore/ytrace.h>
#include <yaafc/yjson/yjson.h>
#include <yaafc/yloop/yloop.h>

#include <picohttpparser.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FE_REQ_BUF      (256 * 1024)
#define FE_MAX_HEADERS  64

struct serve_ud {
    struct yloop *loop;
    const struct frontend_config *cfg;
    struct gateway_url gw;
};

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

static const char *http_reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 303: return "See Other";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    case 502: return "Bad Gateway";
    default:  return "OK";
    }
}

static void send_response(struct yloop_stream *s, int status,
                          const char *content_type,
                          const char *body, size_t body_len,
                          const char *extra_headers, int keep_alive)
{
    char header[1024];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "%s"
        "\r\n",
        status, http_reason(status),
        content_type,
        body_len,
        keep_alive ? "keep-alive" : "close",
        extra_headers ? extra_headers : "");
    if (n <= 0) return;
    yloop_write(s, header, (size_t)n);
    if (body_len && body) yloop_write(s, body, body_len);
}

static void send_redirect(struct yloop_stream *s, const char *location,
                          const char *extra_headers, int keep_alive)
{
    char hdrs[512];
    snprintf(hdrs, sizeof(hdrs), "Location: %s\r\n%s",
             location, extra_headers ? extra_headers : "");
    send_response(s, 303, "text/plain", "", 0, hdrs, keep_alive);
}

static void send_text(struct yloop_stream *s, int status,
                      const char *body, int keep_alive)
{
    send_response(s, status, "text/plain; charset=utf-8",
                  body, strlen(body), NULL, keep_alive);
}

/* ---- static files --------------------------------------------------- */

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

static int serve_static(struct yloop_stream *s, const char *root,
                        const char *url_path, int keep_alive)
{
    if (!root || !*root) return 0;
    /* Strip a leading "/static/" prefix if present — that's the URL
     * convention for served assets in frontend.py. */
    const char *rel = url_path;
    if (strncmp(rel, "/static/", 8) == 0) rel += 8;
    else if (*rel == '/') rel += 1;
    if (!*rel || strstr(rel, "..")) return 0;

    char path[1024];
    int n = snprintf(path, sizeof(path), "%s/%s", root, rel);
    if (n <= 0 || (size_t)n >= sizeof(path)) return 0;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return 0;
    }
    size_t sz = (size_t)st.st_size;
    char *body = malloc(sz);
    if (!body) { close(fd); return 0; }
    size_t got = 0;
    while (got < sz) {
        ssize_t r = read(fd, body + got, sz - got);
        if (r <= 0) break;
        got += (size_t)r;
    }
    close(fd);
    send_response(s, 200, mime_for(path), body, got, NULL, keep_alive);
    free(body);
    return 1;
}

/* ---- routes (proof-of-life subset) --------------------------------- */

/* Hardcoded login page until the template renderer lands. The shape
 * matches yaapp's login.html minimally so the smoke can find the
 * <h1>Sign in</h1>, the username/password fields, and an htmx ref. */
static const char LOGIN_HTML[] =
    "<!doctype html><html><head>"
    "<meta charset=\"utf-8\"><title>Sign in — yaafc-frontend</title>"
    "<script src=\"https://unpkg.com/htmx.org@1.9.10\"></script>"
    "<link rel=\"stylesheet\" href=\"/static/style.css\">"
    "</head><body>"
    "<main class=\"login\">"
    "<h1>Sign in</h1>"
    "<form method=\"post\" action=\"/login\">"
    "<label>Username <input name=\"username\" autofocus></label>"
    "<label>Password <input type=\"password\" name=\"password\"></label>"
    "<button type=\"submit\">Sign in</button>"
    "</form>"
    "<p><a href=\"/register\">Create an account</a></p>"
    "</main>"
    "</body></html>";

static void route_login_get(struct yloop_stream *s, int keep_alive)
{
    send_response(s, 200, "text/html; charset=utf-8",
                  LOGIN_HTML, sizeof(LOGIN_HTML) - 1, NULL, keep_alive);
}

/* HTML-escape `src` into `dst[cap]`. Writes a NUL-terminated string.
 * Returns the number of bytes written (excluding NUL). On overflow,
 * silently truncates at a safe boundary — the caller picks a generous
 * cap. Escapes the OWASP "5 chars in text/attribute context" set so the
 * result is safe in both element bodies and double-quoted attributes. */
static size_t html_escape(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) return 0;
    size_t n = 0;
    for (const char *p = src ? src : ""; *p; ++p) {
        const char *rep = NULL;
        size_t rl = 0;
        switch (*p) {
        case '<':  rep = "&lt;";   rl = 4; break;
        case '>':  rep = "&gt;";   rl = 4; break;
        case '&':  rep = "&amp;";  rl = 5; break;
        case '"':  rep = "&quot;"; rl = 6; break;
        case '\'': rep = "&#39;";  rl = 5; break;
        }
        if (rep) {
            if (n + rl + 1 > cap) break;
            memcpy(dst + n, rep, rl);
            n += rl;
        } else {
            if (n + 1 + 1 > cap) break;
            dst[n++] = *p;
        }
    }
    dst[n] = 0;
    return n;
}

/* Render the login page with an error message above the form. The
 * static template above doesn't have a slot, so we splice the error
 * banner in before <form>. The `err` string is HTML-escaped before
 * interpolation — gateway errors routinely echo user-supplied content
 * (e.g. "no such user 'alice'") and reflecting that unescaped would be
 * stored/reflected XSS. */
static void route_login_get_with_error(struct yloop_stream *s,
                                       const char *err, int keep_alive)
{
    char escaped[1024];
    html_escape(escaped, sizeof(escaped),
                err && *err ? err : "Sign-in failed");
    char body[4096];
    int n = snprintf(body, sizeof(body),
        "<!doctype html><html><head>"
        "<meta charset=\"utf-8\"><title>Sign in — yaafc-frontend</title>"
        "<link rel=\"stylesheet\" href=\"/static/style.css\">"
        "</head><body><main class=\"login\">"
        "<h1>Sign in</h1>"
        "<p class=\"error\">%s</p>"
        "<form method=\"post\" action=\"/login\">"
        "<label>Username <input name=\"username\" autofocus></label>"
        "<label>Password <input type=\"password\" name=\"password\"></label>"
        "<button type=\"submit\">Sign in</button>"
        "</form>"
        "</main></body></html>",
        escaped);
    if (n <= 0) return;
    send_response(s, 200, "text/html; charset=utf-8",
                  body, (size_t)n, NULL, keep_alive);
}

/* URL-decode in place. `s` is mutated; returns the new length. */
static size_t url_decode(char *s)
{
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '+') { *dst++ = ' '; src++; continue; }
        if (*src == '%' && src[1] && src[2]) {
            char hi = src[1], lo = src[2];
            int h = (hi >= '0' && hi <= '9') ? hi - '0'
                  : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10
                  : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
            int l = (lo >= '0' && lo <= '9') ? lo - '0'
                  : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10
                  : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
            if (h >= 0 && l >= 0) {
                *dst++ = (char)((h << 4) | l);
                src += 3;
                continue;
            }
        }
        *dst++ = *src++;
    }
    *dst = 0;
    return (size_t)(dst - s);
}

/* application/x-www-form-urlencoded → field extractor. Returns a
 * newly-allocated NUL-terminated decoded string, or NULL if not found. */
static char *form_get(const char *body, size_t blen, const char *key)
{
    size_t klen = strlen(key);
    const char *p = body, *end = body + blen;
    while (p < end) {
        const char *amp = memchr(p, '&', (size_t)(end - p));
        const char *seg_end = amp ? amp : end;
        const char *eq = memchr(p, '=', (size_t)(seg_end - p));
        if (eq) {
            size_t name_len = (size_t)(eq - p);
            if (name_len == klen && memcmp(p, key, klen) == 0) {
                size_t vlen = (size_t)(seg_end - eq - 1);
                char *out = malloc(vlen + 1);
                if (!out) return NULL;
                memcpy(out, eq + 1, vlen);
                out[vlen] = 0;
                url_decode(out);
                return out;
            }
        }
        if (!amp) break;
        p = amp + 1;
    }
    return NULL;
}

/* Read `n` bytes off the stream into `dst` (allocated by caller). */
static int read_body(struct yloop_stream *s, char *dst, size_t n)
{
    size_t got = 0;
    while (got < n) {
        size_t k = yloop_read_some(s, dst + got, n - got);
        if (k == 0) return -1;
        got += k;
    }
    return 0;
}

static long header_content_length(const struct phr_header *hdrs, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (hdrs[i].name_len != 14) continue;
        int eq = 1;
        for (size_t j = 0; j < 14; ++j) {
            char a = hdrs[i].name[j], b = "Content-Length"[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { eq = 0; break; }
        }
        if (!eq) continue;
        char tmp[32];
        size_t cl = hdrs[i].value_len < sizeof(tmp) - 1
                        ? hdrs[i].value_len : sizeof(tmp) - 1;
        memcpy(tmp, hdrs[i].value, cl);
        tmp[cl] = 0;
        return atol(tmp);
    }
    return -1;
}

/* Cookie header parsing: pluck the value of `name=` out of a "Cookie:"
 * header value (possibly multi-cookie semicolon-separated). Returns
 * the value as a newly-allocated string, or NULL. */
static char *cookie_get(const struct phr_header *hdrs, size_t n,
                        const char *name)
{
    size_t name_len = strlen(name);
    for (size_t i = 0; i < n; ++i) {
        if (hdrs[i].name_len != 6) continue;
        int eq = 1;
        for (size_t j = 0; j < 6; ++j) {
            char a = hdrs[i].name[j], b = "Cookie"[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { eq = 0; break; }
        }
        if (!eq) continue;
        const char *p = hdrs[i].value;
        const char *end = p + hdrs[i].value_len;
        while (p < end) {
            while (p < end && (*p == ' ' || *p == ';')) p++;
            const char *eqsign = memchr(p, '=', (size_t)(end - p));
            const char *semi = memchr(p, ';', (size_t)(end - p));
            const char *kend = (eqsign && (!semi || eqsign < semi)) ? eqsign : semi;
            if (!kend) break;
            size_t kl = (size_t)(kend - p);
            if (kl == name_len && memcmp(p, name, name_len) == 0 && kend == eqsign) {
                const char *v = eqsign + 1;
                const char *vend = semi ? semi : end;
                size_t vl = (size_t)(vend - v);
                char *out = malloc(vl + 1);
                if (!out) return NULL;
                memcpy(out, v, vl);
                out[vl] = 0;
                return out;
            }
            if (!semi) break;
            p = semi + 1;
        }
    }
    return NULL;
}

/* POST /login — call gateway, relay Set-Cookie, redirect or render error.
 *
 * Wire shape sent to gateway:
 *   POST /_rpc
 *   { "path": "session.session.start",
 *     "kwargs": {"method":"password","username":"...","password":"..."} }
 *
 * This is the yaapp path. The yaafc gateway will need to expose the
 * same path before it works there. */
static void route_login_post(struct yloop *loop, struct yloop_stream *s,
                             const struct serve_ud *sud,
                             const char *body, size_t body_len, int keep_alive)
{
    char *username = form_get(body, body_len, "username");
    char *password = form_get(body, body_len, "password");
    if (!username || !password) {
        free(username); free(password);
        route_login_get_with_error(s, "missing username or password", keep_alive);
        return;
    }

    struct yjson_writer *w = yjson_writer_new();
    yjson_w_begin_object(w);
    yjson_w_key(w, "path");  yjson_w_string(w, "session.session.start");
    yjson_w_key(w, "kwargs");
    yjson_w_begin_object(w);
    yjson_w_key(w, "method");   yjson_w_string(w, "password");
    yjson_w_key(w, "username"); yjson_w_string(w, username);
    yjson_w_key(w, "password"); yjson_w_string(w, password);
    yjson_w_end_object(w);
    yjson_w_end_object(w);

    size_t rpc_len;
    const char *rpc_body = yjson_w_data(w, &rpc_len);

    struct http_response resp;
    int rc = http_post_json(loop, &sud->gw, "/_rpc",
                            NULL, NULL, rpc_body, rpc_len, &resp);
    yjson_writer_free(w);
    free(username); free(password);

    if (rc != 0 || resp.status >= 500) {
        http_response_free(&resp);
        route_login_get_with_error(s, "gateway unreachable", keep_alive);
        return;
    }

    if (resp.status >= 400 || resp.status == 0) {
        /* Parse {"error": …} for the message. */
        char err_msg[256] = "sign-in failed";
        if (resp.body) {
            struct yjson_doc *doc = yjson_parse(resp.body, resp.body_len);
            if (doc) {
                const struct yjson_value *root = yjson_doc_root(doc);
                const struct yjson_value *e = yjson_object_get(root, "error");
                const char *em = yjson_as_string(e, NULL);
                if (em) snprintf(err_msg, sizeof(err_msg), "%s", em);
                yjson_doc_free(doc);
            }
        }
        http_response_free(&resp);
        route_login_get_with_error(s, err_msg, keep_alive);
        return;
    }

    /* Success — relay the gateway's Set-Cookie (if any), redirect. */
    char hdrs[1024] = {0};
    if (resp.set_cookie[0]) {
        snprintf(hdrs, sizeof(hdrs), "Set-Cookie: %s\r\n", resp.set_cookie);
    }
    send_redirect(s, "/", hdrs, keep_alive);
    http_response_free(&resp);
}

static int starts_with(const char *p, size_t pl, const char *pref)
{
    size_t n = strlen(pref);
    return pl >= n && memcmp(p, pref, n) == 0;
}

static int path_equals(const char *p, size_t pl, const char *want)
{
    size_t n = strlen(want);
    return pl == n && memcmp(p, want, n) == 0;
}

/* ---- per-peer serve coro -------------------------------------------- */

static void serve_one(struct yloop *l, struct yloop_stream *s, void *ud)
{
    (void)l;
    struct serve_ud *sud = ud;
    const struct frontend_config *cfg = sud->cfg;

    /* Read request bytes until picohttpparser is satisfied. */
    char *buf = malloc(FE_REQ_BUF);
    if (!buf) { yloop_close(s); return; }
    size_t total = 0, last = 0;
    const char *method = NULL, *path = NULL;
    size_t method_len = 0, path_len = 0;
    int minor_version = 0;
    struct phr_header headers[FE_MAX_HEADERS];
    size_t num_headers;

    while (total < FE_REQ_BUF) {
        size_t got = yloop_read_some(s, buf + total, FE_REQ_BUF - total);
        if (got == 0) { free(buf); yloop_close(s); return; }
        total += got;
        num_headers = FE_MAX_HEADERS;
        int r = phr_parse_request(buf, total, &method, &method_len,
                                  &path, &path_len, &minor_version,
                                  headers, &num_headers, last);
        if (r > 0) break;
        if (r == -1) { free(buf); yloop_close(s); return; }
        last = total;
    }

    char ka_hdr[32] = {0};
    int keep_alive = (minor_version >= 1);
    if (header_match(headers, num_headers, "connection", ka_hdr, sizeof(ka_hdr))) {
        if (strstr(ka_hdr, "close")) keep_alive = 0;
        if (strstr(ka_hdr, "keep-alive")) keep_alive = 1;
    }

    /* Method+path dispatch. Only enough for the proof-of-life today —
     * /login GET, static, /, everything else 404. */
    if (method_len == 3 && memcmp(method, "GET", 3) == 0) {
        if (path_equals(path, path_len, "/")) {
            send_redirect(s, "/login", NULL, keep_alive);
        } else if (path_equals(path, path_len, "/login")) {
            route_login_get(s, keep_alive);
        } else if (starts_with(path, path_len, "/static/")) {
            char tmp[1024];
            size_t copy = path_len < sizeof(tmp) - 1 ? path_len : sizeof(tmp) - 1;
            memcpy(tmp, path, copy); tmp[copy] = 0;
            if (!serve_static(s, cfg->static_dir, tmp, keep_alive))
                send_text(s, 404, "not found\n", keep_alive);
        } else {
            send_text(s, 404, "not found\n", keep_alive);
        }
    } else if (method_len == 4 && memcmp(method, "POST", 4) == 0) {
        /* Pull the body off the wire so route handlers see the
         * form-encoded payload as a contiguous buffer. */
        long cl = header_content_length(headers, num_headers);
        if (cl < 0 || cl > 1 << 20) {
            send_text(s, 400, "missing or too-large Content-Length\n", keep_alive);
        } else {
            /* Body may already be partly buffered after the parser
             * accepted the header. picohttpparser returns the offset
             * past CRLF-CRLF as `r` (the parser result we threw away
             * — re-derive by re-running once). */
            size_t hdr_end = 0;
            {
                num_headers = FE_MAX_HEADERS;
                int rr = phr_parse_request(buf, total, &method, &method_len,
                                           &path, &path_len, &minor_version,
                                           headers, &num_headers, 0);
                if (rr > 0) hdr_end = (size_t)rr;
            }
            size_t buffered = total - hdr_end;
            char *body_buf = malloc((size_t)cl + 1);
            if (!body_buf) {
                send_text(s, 500, "out of memory\n", keep_alive);
            } else {
                size_t copy = buffered < (size_t)cl ? buffered : (size_t)cl;
                memcpy(body_buf, buf + hdr_end, copy);
                if (copy < (size_t)cl) {
                    if (read_body(s, body_buf + copy, (size_t)cl - copy) < 0) {
                        free(body_buf);
                        send_text(s, 400, "short body\n", keep_alive);
                        goto post_done;
                    }
                }
                body_buf[cl] = 0;

                if (path_equals(path, path_len, "/login")) {
                    route_login_post(l, s, sud, body_buf, (size_t)cl, keep_alive);
                } else {
                    send_text(s, 404, "not found\n", keep_alive);
                }
                free(body_buf);
            }
        }
    post_done:
        (void)0;
    } else {
        send_text(s, 405, "method not allowed\n", keep_alive);
    }

    free(buf);
    yloop_close(s);
}

/* ---- public entry --------------------------------------------------- */

struct yaafc_void_result frontend_start(struct yloop *loop,
                                        const char *host, int port,
                                        const struct frontend_config *cfg)
{
    /* `ud` outlives the listener: leaked intentionally — process-lifetime,
     * tiny, freed on exit by the OS. The config strings come from yargv
     * which holds them for the chain's lifetime (== whole process). */
    struct serve_ud *ud = calloc(1, sizeof(*ud));
    if (!ud) return YAAFC_ERR(yaafc_void, "frontend_start: calloc failed");
    ud->loop = loop;
    ud->cfg  = cfg;
    if (gateway_url_parse(cfg->gateway_url, &ud->gw) != 0) {
        free(ud);
        return YAAFC_ERR(yaafc_void,
                         "frontend_start: cannot parse --gateway-url");
    }

    struct yaafc_void_result r =
        yloop_listen_tcp(loop, host, port, serve_one, ud);
    YAAFC_RETURN_IF_ERR(yaafc_void, r, "frontend_start: yloop_listen_tcp");
    return YAAFC_OK_VOID();
}
