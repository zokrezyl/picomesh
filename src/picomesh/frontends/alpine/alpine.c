/* alpine — generic service-console frontend (gh#15).
 *
 * Selected with `frontend: alpine`. Serves the generic `/_alpine` service
 * console (a self-contained HTML/JS page) and proxies the console's JSON
 * calls to a configured upstream yhttp endpoint (a yrpc->yhttp bridge or
 * the gateway):
 *
 *   GET  /            -> the console page
 *   GET  /_alpine     -> the console page
 *   *    /_describe[_tree], /<path>/_describe[_tree], POST /_rpc
 *                     -> proxied verbatim to upstream over yhttp
 *
 * The page knows nothing about any plugin or route: it builds its whole
 * UI from `/_describe` and invokes through JSON `/_rpc`, so it works
 * against any yhttp-compatible endpoint. It is neither the transport
 * bridge nor the picoforge webapp.
 *
 * Access control is explicit: this console can reach every service the
 * upstream exposes, so the node binds 127.0.0.1 by default and, when a
 * `token` is configured, every request must carry it (Authorization:
 * Bearer <token> or ?token=<token>). The console page bootstraps the
 * token from its own `?token=` query and replays it as a bearer header. */

#define _POSIX_C_SOURCE 200809L

#include <picomesh/frontends/alpine/alpine.h>
#include <picomesh/yengine/engine.h>
#include <picomesh/yloop/yloop.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/yargv/yargv.h>
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>

#include <picohttpparser.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPINE_REQ_BUF     (256 * 1024)
#define ALPINE_RESP_BUF    (1024 * 1024)
#define ALPINE_MAX_HEADERS 64

struct alpine_frontend {
    struct picomesh_engine *engine;
    char up_host[128];
    int up_port;
    char *token; /* NULL when no token configured */
};

/* ---- the console page (self-contained: no external scripts) ---------- */

static const char ALPINE_CONSOLE_HTML[] =
"<!doctype html>\n"
"<html lang='en'><head><meta charset='utf-8'>\n"
"<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
"<title>picomesh service console</title>\n"
"<style>\n"
" :root{--bg:#0b1014;--fg:#e0e5e4;--mut:#9fa7a8;--acc:#6ba892;--row:#1e262c;--bd:#364a47}\n"
" *{box-sizing:border-box}\n"
" body{margin:0;font:14px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--fg)}\n"
" header{padding:10px 18px;border-bottom:1px solid var(--bd);display:flex;gap:12px;align-items:center}\n"
" header h1{font-size:15px;margin:0;color:var(--acc)}\n"
" .ep{color:var(--mut);font-size:12px}\n"
" button{background:var(--row);color:var(--fg);border:1px solid var(--bd);border-radius:4px;padding:4px 8px;cursor:pointer}\n"
" button:hover{border-color:var(--acc)}\n"
" main{display:flex;align-items:flex-start}\n"
" #tree{flex:1;padding:14px 18px;max-height:calc(100vh - 48px);overflow:auto}\n"
" #panel{width:44%;padding:14px 18px;border-left:1px solid var(--bd);min-height:calc(100vh - 48px)}\n"
" .service{margin:0 0 14px}\n"
" .service h2{font-size:14px;margin:0 0 2px}\n"
" .badge{font-size:11px;color:var(--mut);border:1px solid var(--bd);border-radius:3px;padding:0 5px;margin-left:6px}\n"
" .klass{margin:4px 0 8px 12px}\n"
" .klass h3{font-size:12px;color:var(--mut);margin:6px 0 4px;font-weight:600}\n"
" .method{margin:2px 4px 2px 0;font-family:ui-monospace,monospace;font-size:12px}\n"
" .path{font-family:ui-monospace,monospace;color:var(--acc);word-break:break-all;margin-bottom:8px}\n"
" textarea{width:100%;height:90px;background:var(--bg);color:var(--fg);border:1px solid var(--bd);border-radius:4px;font-family:ui-monospace,monospace;padding:6px}\n"
" pre{background:#000;border:1px solid var(--bd);border-radius:4px;padding:8px;overflow:auto;max-height:42vh;white-space:pre-wrap;word-break:break-all}\n"
" .err{color:#e06c6c}\n"
"</style></head>\n"
"<body>\n"
"<header><h1>picomesh service console</h1><span class='ep' id='ep'></span>"
"<button id='reload'>reload</button></header>\n"
"<main><div id='tree'>loading...</div>"
"<div id='panel'><p class='ep'>select a method</p></div></main>\n"
"<script>\n"
"var TOKEN=new URLSearchParams(location.search).get('token');\n"
"function H(x){var h=x||{};if(TOKEN)h['Authorization']='Bearer '+TOKEN;return h;}\n"
"function el(t,c,txt){var e=document.createElement(t);if(c)e.className=c;if(txt!=null)e.textContent=txt;return e;}\n"
"async function describe(){var r=await fetch('/_describe',{method:'POST',headers:H({'Content-Type':'application/json'}),body:'{}'});if(!r.ok)throw new Error('/_describe HTTP '+r.status);return r.json();}\n"
"async function rpc(p,a){var r=await fetch('/_rpc',{method:'POST',headers:H({'Content-Type':'application/json'}),body:JSON.stringify({path:p,args:a,kwargs:{}})});var t=await r.text();var d;try{d=JSON.parse(t);}catch(e){d=t;}return{status:r.status,ok:r.ok,data:d};}\n"
"function panel(p){var pn=document.getElementById('panel');pn.innerHTML='';pn.appendChild(el('div','path',p));var ta=el('textarea');ta.value='[]';pn.appendChild(ta);var b=el('button',null,'invoke');pn.appendChild(b);var out=el('pre');pn.appendChild(out);b.onclick=async function(){var a;try{a=JSON.parse(ta.value||'[]');}catch(e){out.className='err';out.textContent='args must be a JSON array: '+e;return;}out.className='';out.textContent='...';try{var res=await rpc(p,a);out.className=res.ok?'':'err';out.textContent='HTTP '+res.status+'\\n'+JSON.stringify(res.data,null,2);}catch(e){out.className='err';out.textContent=String(e);}};}\n"
"function verb(c,m){var p=(c.qname||'')+'_';return (p.length>1&&m.indexOf(p)===0)?m.slice(p.length):m;}\n"
"function render(doc){var root=document.getElementById('tree');root.innerHTML='';var svcs=(doc&&doc.services)||[];if(!svcs.length){root.textContent='no active services';return;}svcs.forEach(function(s){var sec=el('div','service');var h2=el('h2',null,s.service);h2.appendChild(el('span','badge',s.source||'?'));sec.appendChild(h2);(s.classes||[]).forEach(function(c){var kb=el('div','klass');kb.appendChild(el('h3',null,c.class));(c.methods||[]).forEach(function(m){var v=verb(c,m);var bt=el('button','method',v);var path=c.class+'.'+v;bt.onclick=function(){panel(path);};kb.appendChild(bt);});sec.appendChild(kb);});root.appendChild(sec);});}\n"
"async function load(){document.getElementById('ep').textContent=location.host;var t=document.getElementById('tree');try{render(await describe());}catch(e){t.className='err';t.textContent=String(e);}}\n"
"document.getElementById('reload').onclick=load;load();\n"
"</script>\n"
"</body></html>\n";

/* ---- tiny HTTP helpers ----------------------------------------------- */

extern size_t yloop_write(struct yloop_stream *s, const void *buf, size_t n);

static void send_resp(struct yloop_stream *s, int status, const char *reason,
                      const char *ctype, const char *body, size_t blen)
{
    char hdr[512];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "\r\n",
        status, reason, ctype, blen);
    if (n <= 0) return;
    yloop_write(s, hdr, (size_t)n);
    if (blen) yloop_write(s, body, blen);
}

static int hdr_match(const struct phr_header *hdrs, size_t n, const char *want,
                     char *out, size_t out_cap)
{
    size_t wl = strlen(want);
    for (size_t i = 0; i < n; ++i) {
        if (hdrs[i].name_len != wl) continue;
        if (strncasecmp(hdrs[i].name, want, wl) != 0) continue;
        size_t copy = hdrs[i].value_len < out_cap - 1 ? hdrs[i].value_len : out_cap - 1;
        memcpy(out, hdrs[i].value, copy);
        out[copy] = 0;
        return 1;
    }
    return 0;
}

/* Compare request path (ignoring any ?query) to a literal. */
static int path_eq(const char *p, const char *target)
{
    const char *q = strchr(p, '?');
    size_t n = q ? (size_t)(q - p) : strlen(p);
    return n == strlen(target) && memcmp(p, target, n) == 0;
}

static int path_ends(const char *p, const char *suffix)
{
    const char *q = strchr(p, '?');
    size_t n = q ? (size_t)(q - p) : strlen(p);
    size_t sl = strlen(suffix);
    return n > sl && memcmp(p + n - sl, suffix, sl) == 0;
}

/* Extract ?key=value from the path. Returns 1 on hit. */
static int query_get(const char *path, const char *key, char *out, size_t out_cap)
{
    const char *q = strchr(path, '?');
    if (!q) return 0;
    size_t kl = strlen(key);
    const char *p = q + 1;
    while (*p) {
        const char *eq = strchr(p, '=');
        const char *amp = strchr(p, '&');
        if (eq && (!amp || eq < amp) && (size_t)(eq - p) == kl && memcmp(p, key, kl) == 0) {
            const char *vend = amp ? amp : p + strlen(p);
            size_t vl = (size_t)(vend - eq - 1);
            if (vl >= out_cap) vl = out_cap - 1;
            memcpy(out, eq + 1, vl);
            out[vl] = 0;
            return 1;
        }
        if (!amp) break;
        p = amp + 1;
    }
    return 0;
}

/* True for the JSON API surface the console drives — proxied upstream. */
static int wants_proxy(const char *p, int is_get, int is_post)
{
    if (is_post && path_eq(p, "/_rpc")) return 1;
    if (is_get || is_post) {
        if (path_eq(p, "/_describe") || path_eq(p, "/_describe_tree")) return 1;
        if (path_ends(p, "/_describe") || path_ends(p, "/_describe_tree")) return 1;
    }
    return 0;
}

static int authorized(const struct alpine_frontend *f, const struct phr_header *hdrs,
                      size_t nh, const char *path)
{
    if (!f->token) return 1; /* no token configured: open (loopback admin tool) */
    char got[300] = {0};
    char auth[300] = {0};
    if (hdr_match(hdrs, nh, "authorization", auth, sizeof(auth))) {
        const char *v = auth;
        if (strncasecmp(v, "Bearer ", 7) == 0) v += 7;
        while (*v == ' ') ++v;
        snprintf(got, sizeof(got), "%s", v);
    }
    if (!got[0]) query_get(path, "token", got, sizeof(got));
    return got[0] && strcmp(got, f->token) == 0;
}

/* Open a fresh upstream connection, forward the request, relay the full
 * close-delimited response back to the browser. The console token (if any)
 * is NOT forwarded — it gates the console, not the upstream. */
static void proxy_upstream(struct yloop *l, struct yloop_stream *client,
                           const struct alpine_frontend *f,
                           const char *method, const char *path,
                           const char *ctype, const char *body, size_t blen)
{
    struct yloop_stream_ptr_result ur = yloop_connect_tcp(l, f->up_host, f->up_port);
    if (PICOMESH_IS_ERR(ur)) {
        picomesh_error_destroy(ur.error);
        static const char msg[] = "{\"error\":\"alpine: upstream connect failed\"}";
        send_resp(client, 502, "Bad Gateway", "application/json", msg, sizeof(msg) - 1);
        return;
    }
    struct yloop_stream *up = ur.value;

    char head[4096];
    int hn = snprintf(head, sizeof(head),
        "%s %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n",
        method, path, f->up_host, f->up_port);
    if (hn > 0 && ctype && *ctype)
        hn += snprintf(head + hn, sizeof(head) - (size_t)hn, "Content-Type: %s\r\n", ctype);
    if (hn > 0)
        hn += snprintf(head + hn, sizeof(head) - (size_t)hn, "Content-Length: %zu\r\n\r\n", blen);
    if (hn <= 0 || (size_t)hn >= sizeof(head)) {
        yloop_close(up);
        static const char msg[] = "{\"error\":\"alpine: request too large to proxy\"}";
        send_resp(client, 502, "Bad Gateway", "application/json", msg, sizeof(msg) - 1);
        return;
    }
    yloop_write(up, head, (size_t)hn);
    if (blen) yloop_write(up, body, blen);

    /* Upstream was asked to close, so read to EOF == full response. */
    char *resp = malloc(ALPINE_RESP_BUF);
    if (!resp) { yloop_close(up); yloop_close(client); return; }
    size_t rl = 0;
    for (;;) {
        if (rl >= ALPINE_RESP_BUF) break;
        size_t got = yloop_read_some(up, resp + rl, ALPINE_RESP_BUF - rl);
        if (got == 0) break;
        rl += got;
    }
    yloop_close(up);
    if (rl) yloop_write(client, resp, rl);
    free(resp);
}

/* ---- per-connection serve coroutine (one request, then close) -------- */

static void serve_one(struct yloop *l, struct yloop_stream *s, void *ud)
{
    struct alpine_frontend *f = ud;
    char *buf = malloc(ALPINE_REQ_BUF);
    if (!buf) { yloop_close(s); return; }

    size_t buf_len = 0;
    int minor = 0;
    const char *m = NULL, *p = NULL;
    size_t ml = 0, pl = 0;
    struct phr_header hdrs[ALPINE_MAX_HEADERS];
    size_t nh = 0;
    int parsed = -2;

    while (parsed == -2) {
        if (buf_len >= ALPINE_REQ_BUF) goto done;
        size_t chunk = ALPINE_REQ_BUF - buf_len;
        if (chunk > 4096) chunk = 4096;
        size_t got = yloop_read_some(s, buf + buf_len, chunk);
        if (got == 0) goto done;
        buf_len += got;
        nh = ALPINE_MAX_HEADERS;
        parsed = phr_parse_request(buf, buf_len, &m, &ml, &p, &pl, &minor, hdrs, &nh, 0);
    }
    if (parsed < 0) goto done;
    size_t header_end = (size_t)parsed;

    long clen = 0;
    char cl[32] = {0};
    if (hdr_match(hdrs, nh, "Content-Length", cl, sizeof(cl))) clen = strtol(cl, NULL, 10);
    if (clen > 0) {
        size_t have = buf_len - header_end;
        while ((long)have < clen) {
            if (header_end + (size_t)clen > ALPINE_REQ_BUF) goto done;
            size_t got = yloop_read_some(s, buf + buf_len, (size_t)clen - have);
            if (got == 0) goto done;
            buf_len += got;
            have += got;
        }
    }
    const char *body = buf + header_end;
    ((char *)p)[pl] = 0; /* NUL-terminate path (byte after is part of HTTP/1.1) */

    char method[16] = {0};
    memcpy(method, m, ml < sizeof(method) - 1 ? ml : sizeof(method) - 1);
    int is_get = strcmp(method, "GET") == 0;
    int is_post = strcmp(method, "POST") == 0;
    int is_opt = strcmp(method, "OPTIONS") == 0;

    if (!authorized(f, hdrs, nh, p)) {
        static const char msg[] =
            "{\"error\":\"unauthorized: pass ?token= or Authorization: Bearer <token>\"}";
        send_resp(s, 401, "Unauthorized", "application/json", msg, sizeof(msg) - 1);
        goto done;
    }

    if (is_opt) {
        send_resp(s, 200, "OK", "text/plain", "", 0);
        goto done;
    }
    if (is_get && (path_eq(p, "/") || path_eq(p, "/_alpine"))) {
        send_resp(s, 200, "OK", "text/html; charset=utf-8",
                  ALPINE_CONSOLE_HTML, sizeof(ALPINE_CONSOLE_HTML) - 1);
        goto done;
    }
    if (wants_proxy(p, is_get, is_post)) {
        char ctype[128] = {0};
        hdr_match(hdrs, nh, "Content-Type", ctype, sizeof(ctype));
        proxy_upstream(l, s, f, method, p, ctype[0] ? ctype : "application/json",
                       body, (size_t)clen);
        goto done;
    }

    {
        static const char nf[] =
            "{\"error\":\"alpine: no such route (serves /_alpine; proxies /_describe and /_rpc)\"}";
        send_resp(s, 404, "Not Found", "application/json", nf, sizeof(nf) - 1);
    }

done:
    free(buf);
    yloop_close(s);
}

/* ---- config + start -------------------------------------------------- */

/* Fetch `alpine.<suffix>` for this node, honoring the engine's service
 * projection (top level) with a fallback to the un-projected parent path. */
static const struct yconfig_node *alpine_cfg(struct picomesh_engine *e, const char *suffix)
{
    const struct yconfig *cfg = picomesh_engine_config(e);
    if (!cfg) return NULL;
    struct yargv_chain *cli = picomesh_engine_cli(e);
    const char *name = cli ? yargv_get_string(cli, "name", NULL) : NULL;
    char path[256];
    if (name && *name) {
        snprintf(path, sizeof(path), "mesh.services.%s.config.alpine.%s", name, suffix);
        struct yconfig_node_ptr_result r = yconfig_get(cfg, path);
        if (PICOMESH_IS_OK(r) && r.value) return r.value;
        if (PICOMESH_IS_ERR(r)) picomesh_error_destroy(r.error);
    }
    snprintf(path, sizeof(path), "alpine.%s", suffix);
    struct yconfig_node_ptr_result r = yconfig_get(cfg, path);
    if (PICOMESH_IS_OK(r) && r.value) return r.value;
    if (PICOMESH_IS_ERR(r)) picomesh_error_destroy(r.error);
    return NULL;
}

struct alpine_upstream_fields {
    char host[128];
    int port;
};

static int alpine_upstream_cb(const char *key, const struct yconfig_node *val, void *ud)
{
    struct alpine_upstream_fields *u = ud;
    if (strcmp(key, "host") == 0) {
        const char *h = yconfig_node_as_string(val, NULL);
        if (h) snprintf(u->host, sizeof(u->host), "%s", h);
    } else if (strcmp(key, "port") == 0) {
        u->port = (int)yconfig_node_as_int(val, 0);
    }
    return 0;
}

struct alpine_frontend_ptr_result alpine_start(struct picomesh_engine *e,
                                               const struct alpine_config *cfg)
{
    if (!e) return PICOMESH_ERR(alpine_frontend_ptr, "alpine_start: NULL engine");
    const char *host = (cfg && cfg->host) ? cfg->host : "127.0.0.1";
    int port = (cfg && cfg->port > 0) ? cfg->port : 8231;

    struct yloop *l = picomesh_engine_loop(e);
    if (!l) return PICOMESH_ERR(alpine_frontend_ptr, "alpine_start: engine has no loop");

    /* Resolve the upstream yhttp endpoint this console proxies to. */
    struct alpine_upstream_fields up = {.host = "127.0.0.1", .port = 0};
    const struct yconfig_node *un = alpine_cfg(e, "upstream");
    if (un && yconfig_node_kind(un) == YCONFIG_MAP)
        yconfig_node_for_each(un, alpine_upstream_cb, &up);
    if (up.port <= 0)
        return PICOMESH_ERR(alpine_frontend_ptr,
                            "alpine_start: config.alpine.upstream.port is required "
                            "(the yhttp endpoint the console proxies to)");

    struct alpine_frontend *f = calloc(1, sizeof(*f));
    if (!f) return PICOMESH_ERR(alpine_frontend_ptr, "alpine_start: calloc failed");
    f->engine = e;
    snprintf(f->up_host, sizeof(f->up_host), "%s", up.host[0] ? up.host : "127.0.0.1");
    f->up_port = up.port;

    const struct yconfig_node *tn = alpine_cfg(e, "token");
    const char *tok = tn ? yconfig_node_as_string(tn, NULL) : NULL;
    if (tok && *tok) {
        f->token = strdup(tok);
        if (!f->token) { free(f); return PICOMESH_ERR(alpine_frontend_ptr, "alpine_start: strdup failed"); }
    }

    struct picomesh_void_result lr = yloop_listen_tcp(l, host, port, serve_one, f);
    if (PICOMESH_IS_ERR(lr)) {
        free(f->token);
        free(f);
        return PICOMESH_ERR(alpine_frontend_ptr, "alpine_start: yloop_listen_tcp failed", lr);
    }
    yinfo("alpine: console on %s:%d -> upstream %s:%d (%s)",
          host, port, f->up_host, f->up_port, f->token ? "token-gated" : "open");
    return PICOMESH_OK(alpine_frontend_ptr, f);
}

void alpine_stop(struct alpine_frontend *f)
{
    if (!f) return;
    free(f->token);
    free(f);
}
