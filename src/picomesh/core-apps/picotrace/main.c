/* picotrace - internal trace browser for Picomesh.
 *
 * This is intentionally NOT part of the public Picoforge webapp. It is a
 * generic core operator app that binds loopback by default and talks directly
 * to a mesh backend service, normally trace_collector, over yrpc.
 */

#define _POSIX_C_SOURCE 200809L

#include <picomesh/plugin/trace_collector/trace_collector.h>
#include <picomesh/yargv/yargv.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/yjson/yjson.h>
#include <picomesh/yloop/yloop.h>

#include <picohttpparser.h>

#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define REQ_BUF 65536
#define MAX_HEADERS 64

struct app_config {
    const char *upstream_service;
    const char *upstream_host;
    int upstream_port;
};

/* Vendored ECharts bundle, embedded at build time (see cmake/bin2c.cmake). */
extern const char picotrace_echarts_js[];
extern const unsigned long picotrace_echarts_js_len;

struct buf { char *data; size_t len, cap; };

static void buf_init(struct buf *b) { b->data = NULL; b->len = 0; b->cap = 0; }
static void buf_free(struct buf *b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }

static int buf_reserve(struct buf *b, size_t extra)
{
    if (b->data && b->len + extra + 1 <= b->cap) return 1;
    size_t nc = b->cap ? b->cap : 4096;
    while (b->len + extra + 1 > nc) nc *= 2;
    char *nd = realloc(b->data, nc);
    if (!nd) return 0;
    b->data = nd; b->cap = nc;
    return 1;
}

static void buf_putn(struct buf *b, const char *s, size_t n)
{
    if (!buf_reserve(b, n)) return;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

static void buf_puts(struct buf *b, const char *s) { buf_putn(b, s, strlen(s)); }

static void buf_printf(struct buf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char tmp[2048];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(tmp)) { buf_putn(b, tmp, (size_t)n); return; }
    char *big = malloc((size_t)n + 1);
    if (!big) return;
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    buf_putn(b, big, (size_t)n);
    free(big);
}

static void buf_esc(struct buf *b, const char *s)
{
    for (const char *p = s ? s : ""; *p; ++p) {
        switch (*p) {
        case '&':  buf_puts(b, "&amp;");  break;
        case '<':  buf_puts(b, "&lt;");   break;
        case '>':  buf_puts(b, "&gt;");   break;
        case '"': buf_puts(b, "&quot;"); break;
        case '\'': buf_puts(b, "&#39;");  break;
        default:   buf_putn(b, p, 1);      break;
        }
    }
}

static void fmt_duration(char *out, size_t cap, uint64_t ns)
{
    if (ns >= 1000000000ull)
        snprintf(out, cap, "%.2fs", (double)ns / 1000000000.0);
    else if (ns >= 1000000ull)
        snprintf(out, cap, "%.2fms", (double)ns / 1000000.0);
    else if (ns >= 1000ull)
        snprintf(out, cap, "%.2fus", (double)ns / 1000.0);
    else
        snprintf(out, cap, "%lluns", (unsigned long long)ns);
}

static char *query_get(const char *path, const char *key)
{
    const char *q = strchr(path, '?');
    if (!q) return NULL;
    q++;
    size_t klen = strlen(key);
    while (*q) {
        const char *amp = strchr(q, '&');
        const char *end = amp ? amp : q + strlen(q);
        const char *eq = memchr(q, '=', (size_t)(end - q));
        if (eq && (size_t)(eq - q) == klen && memcmp(q, key, klen) == 0) {
            size_t len = (size_t)(end - eq - 1);
            char *out = malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, eq + 1, len);
            out[len] = 0;
            for (char *p = out; *p; ++p) if (*p == '+') *p = ' ';
            return out;
        }
        if (!amp) break;
        q = amp + 1;
    }
    return NULL;
}

static const char *reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    default: return "OK";
    }
}

static void send_response(struct yloop_stream *s, int status, const char *ctype,
                          const char *body, size_t body_len)
{
    char hdr[512];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        status, reason(status), ctype, body_len);
    if (n > 0) yloop_write(s, hdr, (size_t)n);
    if (body && body_len) yloop_write(s, body, body_len);
}

static int collector_connect(const char *host, int port)
{
    if (!host || !*host) host = "127.0.0.1";
    const char *dial = (strcmp(host, "0.0.0.0") == 0) ? "127.0.0.1" : host;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, dial, &addr.sin_addr) != 1) { close(fd); return -1; }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    return fd;
}

struct collector_session {
    struct peer_channel *channel;
    struct object *obj;
    struct ctx ctx;
};

static int collector_open(const struct app_config *cfg, struct collector_session *session)
{
    memset(session, 0, sizeof(*session));
    if (!cfg || cfg->upstream_port <= 0) return 0;
    int fd = collector_connect(cfg->upstream_host, cfg->upstream_port);
    if (fd < 0) return 0;
    session->channel = peer_channel_create(fd);
    if (!session->channel) { close(fd); return 0; }
    session->ctx = (struct ctx){.peer = session->channel};
    struct object_ptr_result obj_r = trace_collector_trace_collector_create(&session->ctx);
    if (PICOMESH_IS_ERR(obj_r)) {
        picomesh_error_destroy(obj_r.error);
        peer_channel_destroy(session->channel);
        session->channel = NULL;
        return 0;
    }
    session->obj = obj_r.value;
    return 1;
}

static void collector_close(struct collector_session *session)
{
    if (!session || !session->channel) return;
    object_release_in_ctx(&session->ctx, session->obj);
    peer_channel_destroy(session->channel);
    session->channel = NULL;
    session->obj = NULL;
}

static char *collector_get_trace(const struct app_config *cfg, const char *trace_id)
{
    struct collector_session s;
    if (!collector_open(cfg, &s)) return NULL;
    struct picomesh_string_result r =
        trace_collector_trace_collector_get_trace(&s.ctx, s.obj, NULL, trace_id ? trace_id : "");
    char *out = NULL;
    if (PICOMESH_IS_OK(r)) out = r.value;
    else picomesh_error_destroy(r.error);
    collector_close(&s);
    return out;
}

static char *collector_traces(const struct app_config *cfg, const char *service,
                              const char *status, uint32_t since_secs)
{
    struct collector_session s;
    if (!collector_open(cfg, &s)) return NULL;
    struct picomesh_string_result r =
        trace_collector_trace_collector_traces(&s.ctx, s.obj, NULL,
                                               service ? service : "", status ? status : "", since_secs);
    char *out = NULL;
    if (PICOMESH_IS_OK(r)) out = r.value;
    else picomesh_error_destroy(r.error);
    collector_close(&s);
    return out;
}

static char *collector_services(const struct app_config *cfg)
{
    struct collector_session s;
    if (!collector_open(cfg, &s)) return NULL;
    struct picomesh_string_result r = trace_collector_trace_collector_services(&s.ctx, s.obj, NULL);
    char *out = NULL;
    if (PICOMESH_IS_OK(r)) out = r.value;
    else picomesh_error_destroy(r.error);
    collector_close(&s);
    return out;
}

static char *collector_stats(const struct app_config *cfg)
{
    struct collector_session s;
    if (!collector_open(cfg, &s)) return NULL;
    struct picomesh_string_result r = trace_collector_trace_collector_stats(&s.ctx, s.obj, NULL);
    char *out = NULL;
    if (PICOMESH_IS_OK(r)) out = r.value;
    else picomesh_error_destroy(r.error);
    collector_close(&s);
    return out;
}

/* Embed a JSON document inside a <script> block. Only '<' has to be
 * neutralized so a literal "</script>" appearing in string data cannot close
 * the element early; escaping it as < keeps the document valid JSON. */
static void buf_put_json_script(struct buf *b, const char *json)
{
    for (const char *p = json; *p; ++p) {
        if (*p == '<') buf_puts(b, "\\u003c");
        else buf_putn(b, p, 1);
    }
}

static void render_head(struct buf *b)
{
    buf_puts(b,
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>picotrace</title>"
        "<script src=\"/echarts.min.js\"></script>"
        "<style>"
        ":root{--bg:#f7f8fa;--fg:#1f2328;--muted:#59636e;--border:#d0d7de;--panel:#fff;--sub:#f3f5f7;--accent:#0969da;--danger:#cf222e;--ok:#1a7f37}"
        "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 -apple-system,BlinkMacSystemFont,Segoe UI,Helvetica,Arial,sans-serif}"
        "header{height:48px;background:#1f2530;color:#e6e9ee;display:flex;align-items:center;padding:0 16px;gap:16px}header strong{font-size:16px}"
        "main{max-width:1280px;margin:0 auto;padding:20px}a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}"
        ".panel{background:var(--panel);border:1px solid var(--border);border-radius:6px;margin:0 0 14px;overflow:hidden}.panel h2{font-size:14px;margin:0;padding:10px 14px;background:var(--sub);border-bottom:1px solid var(--border)}.panel-body{padding:14px}"
        "form{display:flex;gap:10px;align-items:flex-end;flex-wrap:wrap}label{display:block;color:var(--muted)}input,select{display:block;margin-top:3px;padding:6px 8px;border:1px solid var(--border);border-radius:6px;background:white}button{padding:7px 12px;border:1px solid var(--accent);border-radius:6px;background:var(--accent);color:white}"
        "table{width:100%;border-collapse:collapse;font-size:13px;table-layout:fixed}th,td{padding:7px 10px;border-bottom:1px solid var(--border);text-align:left;vertical-align:middle}th{background:var(--sub);color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.03em}code,.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace;overflow-wrap:anywhere}"
        ".badge{display:inline-block;border:1px solid var(--border);border-radius:999px;padding:1px 7px;font-size:12px}.ok{color:var(--ok);background:#dafbe1;border-color:#b5e6c6}.err{color:var(--danger);background:#ffebe9;border-color:#ffcecb}.muted{color:var(--muted)}"
        "#trace-chart{width:100%;min-height:200px}"
        ".crumb{margin:2px 0 14px;font-size:13px}.trace-row{cursor:pointer}.trace-row:hover{background:#f0f6ff}"
        "</style></head><body><header><strong>picotrace</strong><span class=\"muted\">internal Picomesh trace browser</span></header><main>");
}

static void render_trace_detail(struct buf *b, const struct app_config *cfg, const char *trace_id)
{
    if (!trace_id || !*trace_id) return;
    char *json = collector_get_trace(cfg, trace_id);
    if (!json) {
        buf_puts(b, "<section class=\"panel\"><h2>Trace detail</h2><div class=\"panel-body\"><p class=\"err\">Cannot load trace.</p></div></section>");
        return;
    }
    /* The server does no span parsing or layout: it hands the raw collector
     * JSON to the browser and lets Apache ECharts build the waterfall
     * client-side. */
    buf_puts(b, "<section class=\"panel\"><h2>Trace detail</h2><div class=\"panel-body\"><p><code>");
    buf_esc(b, trace_id);
    buf_puts(b, "</code> <span id=\"trace-meta\" class=\"muted\"></span></p>");
    buf_puts(b, "<div id=\"trace-chart\"></div>");
    buf_puts(b, "<script type=\"application/json\" id=\"trace-data\">");
    buf_put_json_script(b, json);
    buf_puts(b, "</script>");
    buf_puts(b, "<script>");
    buf_puts(b,
        "(function(){\n"
        "var el=document.getElementById('trace-data');\n"
        "var container=document.getElementById('trace-chart');\n"
        "if(!el||!container)return;\n"
        "if(typeof echarts==='undefined'){container.innerHTML='<p class=err>ECharts failed to load (needs network access for the CDN).</p>';return;}\n"
        "var trace=JSON.parse(el.textContent);\n"
        "var spans=(trace&&trace.spans)||[];\n"
        "if(!spans.length){container.innerHTML='<p class=muted>No spans found.</p>';return;}\n"
        "var byId={};for(var i=0;i<spans.length;i++)byId[spans[i].span_id]=spans[i];\n"
        "var minStart=null,maxEnd=0;\n"
        "for(var i=0;i<spans.length;i++){var st=Number(spans[i].start_time_ns||0),d=Number(spans[i].duration_ns||0);if(minStart===null||st<minStart)minStart=st;if(st+d>maxEnd)maxEnd=st+d;}\n"
        "if(minStart===null)minStart=0;\n"
        "var total=maxEnd>minStart?maxEnd-minStart:1;\n"
        "var children={},roots=[];\n"
        "for(var i=0;i<spans.length;i++){var s=spans[i],p=s.parent_span_id;if(p&&byId[p]&&p!==s.span_id){(children[p]=children[p]||[]).push(s);}else{roots.push(s);}}\n"
        "function byStart(a,b){return Number(a.start_time_ns||0)-Number(b.start_time_ns||0);}\n"
        "var ordered=[],depthOf={};\n"
        "(function walk(list,depth){list.sort(byStart);for(var i=0;i<list.length;i++){var s=list[i];ordered.push(s);depthOf[s.span_id]=depth;if(children[s.span_id])walk(children[s.span_id],depth+1);}})(roots,0);\n"
        "var palette=['#0072c3','#eb6200','#8a3ffc','#b28600','#005d5d','#fa4d56','#198038','#9f1853','#002d9c','#6f6f6f','#00539c','#8a3800','#6929c4','#8e6a00','#002d2d','#570408','#0e6027','#510224','#001141','#491d8b'];\n"
        "var svcIdx={},svcN=0;\n"
        "function colorFor(svc){if(!(svc in svcIdx))svcIdx[svc]=svcN++;return palette[svcIdx[svc]%palette.length];}\n"
        "function fmtDur(ns){ns=Number(ns)||0;if(ns<1000)return ns+'ns';if(ns<1e6)return (ns/1e3).toFixed(1)+'\xc2\xb5s';if(ns<1e9)return (ns/1e6).toFixed(2)+'ms';return (ns/1e9).toFixed(2)+'s';}\n"
        "function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}\n"
        "var categories=[],data=[];\n"
        "for(var r=0;r<ordered.length;r++){var s=ordered[r],st=Number(s.start_time_ns||0)-minStart,d=Number(s.duration_ns||0),depth=depthOf[s.span_id]||0;var pad='';for(var k=0;k<depth;k++)pad+='\\u00a0\\u00a0';categories.push(pad+(s.name||'(unnamed)'));data.push({value:[r,st,st+d,s.status==='error'?1:0],itemStyle:{color:colorFor(s.service_name||'')},span:s});}\n"
        "function renderItem(params,api){var row=api.value(0);var start=api.coord([api.value(1),row]);var end=api.coord([api.value(2),row]);var h=api.size([0,1])[1]*0.6;var w=end[0]-start[0];if(w<1)w=1;var cs=params.coordSys;var x=Math.max(start[0],cs.x);var x2=Math.min(start[0]+w,cs.x+cs.width);if(x2<=x)return null;var style=api.style();if(api.value(3))style=Object.assign({},style,{stroke:'#cf222e',lineWidth:1});return{type:'rect',shape:{x:x,y:start[1]-h/2,width:x2-x,height:h,r:2},style:style};}\n"
        "var meta=document.getElementById('trace-meta');if(meta)meta.textContent=fmtDur(total)+' \xc2\xb7 '+spans.length+' spans';\n"
        "container.style.height=Math.max(220,ordered.length*24+90)+'px';\n"
        "var chart=echarts.init(container);\n"
        "chart.setOption({animation:false,grid:{left:270,right:24,top:20,bottom:54},"
        "tooltip:{trigger:'item',formatter:function(p){var s=p.data.span;return '<b>'+esc(s.name||'')+'</b><br/>'+(s.service_name?esc(s.service_name)+'<br/>':'')+'duration '+fmtDur(s.duration_ns)+'<br/>start +'+fmtDur(Number(s.start_time_ns||0)-minStart)+(s.status==='error'?'<br/><span style=\\\"color:#cf222e\\\">error</span>':'');}},"
        "xAxis:{type:'value',min:0,max:total,axisLabel:{formatter:function(v){return fmtDur(v);}},splitLine:{show:true,lineStyle:{color:'#eef1f4'}}},"
        "yAxis:{type:'category',data:categories,inverse:true,axisTick:{show:false},axisLine:{show:false},axisLabel:{interval:0,color:'#1f2328',fontSize:12,width:230,overflow:'truncate',align:'left',margin:236}},"
        "dataZoom:[{type:'inside',xAxisIndex:0,filterMode:'none'},{type:'slider',xAxisIndex:0,filterMode:'none',height:20,bottom:14,labelFormatter:function(v){return fmtDur(v);}}],"
        "series:[{type:'custom',renderItem:renderItem,encode:{x:[1,2],y:0},data:data}]});\n"
        "window.addEventListener('resize',function(){chart.resize();});\n"
        "})();\n");
    buf_puts(b, "</script></div></section>");
    free(json);
}

static void render_page(struct yloop_stream *s, struct yloop *loop, const struct app_config *cfg,
                        const char *full_path)
{
    (void)loop;
    char *service = query_get(full_path, "service");
    char *status = query_get(full_path, "status");
    char *since_s = query_get(full_path, "since");
    char *trace = query_get(full_path, "trace");
    long since = since_s && *since_s ? atol(since_s) : 3600;
    if (since < 0) since = 0;

    struct buf b; buf_init(&b);
    render_head(&b);

    if (trace && *trace) {
        /* Dedicated trace view (Jaeger-style): the waterfall is the whole
         * page, reached by clicking a row in the results list. A breadcrumb
         * returns to the search list — no results table underneath. */
        buf_puts(&b, "<p class=\"crumb\"><a href=\"/\">&larr; Search</a></p>");
        render_trace_detail(&b, cfg, trace);
    } else {
        /* Search / results view: filter form, collector stats, results list. */
        char *traces_json = collector_traces(cfg, service ? service : "", status ? status : "", (uint32_t)since);
        char *services_json = collector_services(cfg);
        char *stats_json = collector_stats(cfg);

        buf_puts(&b, "<section class=\"panel\"><h2>Search</h2><div class=\"panel-body\"><form method=\"get\" action=\"/\">");
        buf_puts(&b, "<label>Service<select name=\"service\"><option value=\"\">All services</option>");
        struct yjson_doc *svc_doc = services_json ? yjson_parse(services_json, strlen(services_json)) : NULL;
        const struct yjson_value *svc_arr = svc_doc ? yjson_object_get(yjson_doc_root(svc_doc), "services") : NULL;
        size_t svc_n = svc_arr ? yjson_array_size(svc_arr) : 0;
        for (size_t i = 0; i < svc_n; ++i) {
            const struct yjson_value *e = yjson_array_at(svc_arr, i);
            const char *name = yjson_as_string(yjson_object_get(e, "service_name"), "");
            buf_puts(&b, "<option value=\""); buf_esc(&b, name); buf_puts(&b, "\"");
            if (service && strcmp(service, name) == 0) buf_puts(&b, " selected");
            buf_puts(&b, ">"); buf_esc(&b, name); buf_puts(&b, "</option>");
        }
        buf_puts(&b, "</select></label><label>Status<select name=\"status\">");
        buf_printf(&b, "<option value=\"\"%s>Any</option>", (!status || !*status) ? " selected" : "");
        buf_printf(&b, "<option value=\"ok\"%s>OK</option>", (status && strcmp(status, "ok") == 0) ? " selected" : "");
        buf_printf(&b, "<option value=\"error\"%s>Error</option>", (status && strcmp(status, "error") == 0) ? " selected" : "");
        buf_puts(&b, "</select></label><label>Lookback<select name=\"since\">");
        const long windows[] = {300, 3600, 21600, 86400, 0};
        const char *labels[] = {"5 minutes", "1 hour", "6 hours", "24 hours", "All"};
        for (size_t i = 0; i < sizeof(windows) / sizeof(windows[0]); ++i)
            buf_printf(&b, "<option value=\"%ld\"%s>%s</option>", windows[i], since == windows[i] ? " selected" : "", labels[i]);
        buf_puts(&b, "</select></label><label>Trace ID<input name=\"trace\" value=\"\"></label><button type=\"submit\">Search</button></form></div></section>");

        if (stats_json) {
            struct yjson_doc *doc = yjson_parse(stats_json, strlen(stats_json));
            if (doc) {
                const struct yjson_value *r = yjson_doc_root(doc);
                buf_puts(&b, "<section class=\"panel\"><h2>Collector</h2><div class=\"panel-body\"><span class=\"muted\">stored</span> ");
                buf_printf(&b, "%lld", (long long)yjson_as_int(yjson_object_get(r, "stored"), 0));
                buf_puts(&b, " &nbsp; <span class=\"muted\">ingested</span> ");
                buf_printf(&b, "%lld", (long long)yjson_as_int(yjson_object_get(r, "ingested"), 0));
                buf_puts(&b, " &nbsp; <span class=\"muted\">evicted</span> ");
                buf_printf(&b, "%lld", (long long)yjson_as_int(yjson_object_get(r, "evicted"), 0));
                buf_puts(&b, "</div></section>");
                yjson_doc_free(doc);
            }
        }

        buf_puts(&b, "<section class=\"panel\"><h2>Recent traces</h2><div class=\"panel-body\">");
        if (!traces_json) {
            buf_puts(&b, "<p class=\"err\">Cannot query trace collector through the configured upstream.</p>");
        } else {
            struct yjson_doc *doc = yjson_parse(traces_json, strlen(traces_json));
            const struct yjson_value *arr = doc ? yjson_object_get(yjson_doc_root(doc), "traces") : NULL;
            size_t n = arr ? yjson_array_size(arr) : 0;
            if (!n) buf_puts(&b, "<p class=\"muted\">No traces match.</p>");
            else {
                buf_puts(&b, "<table><thead><tr><th>Trace</th><th>Root</th><th>Service</th><th>Duration</th><th>Spans</th><th>Status</th></tr></thead><tbody>");
                for (size_t i = 0; i < n; ++i) {
                    const struct yjson_value *tr = yjson_array_at(arr, i);
                    const char *tid = yjson_as_string(yjson_object_get(tr, "trace_id"), "");
                    const char *root = yjson_as_string(yjson_object_get(tr, "root_name"), "");
                    const char *svc = yjson_as_string(yjson_object_get(tr, "service_name"), "");
                    const char *st = yjson_as_string(yjson_object_get(tr, "status"), "ok");
                    uint64_t dns = (uint64_t)yjson_as_int(yjson_object_get(tr, "duration_ns"), 0);
                    char dur[32]; fmt_duration(dur, sizeof(dur), dns);
                    /* Whole row links to the dedicated trace view. */
                    buf_puts(&b, "<tr class=\"trace-row\" onclick=\"location='/?trace="); buf_esc(&b, tid);
                    buf_puts(&b, "'\"><td><a class=\"mono\" href=\"/?trace="); buf_esc(&b, tid); buf_puts(&b, "\">"); buf_esc(&b, tid); buf_puts(&b, "</a></td><td>");
                    buf_esc(&b, root); buf_puts(&b, "</td><td>"); buf_esc(&b, svc); buf_puts(&b, "</td><td>"); buf_esc(&b, dur); buf_puts(&b, "</td><td>");
                    buf_printf(&b, "%lld", (long long)yjson_as_int(yjson_object_get(tr, "span_count"), 0));
                    buf_puts(&b, "</td><td><span class=\"badge "); buf_puts(&b, strcmp(st, "error") == 0 ? "err" : "ok"); buf_puts(&b, "\">"); buf_esc(&b, st); buf_puts(&b, "</span></td></tr>");
                }
                buf_puts(&b, "</tbody></table>");
            }
            if (doc) yjson_doc_free(doc);
        }
        buf_puts(&b, "</div></section>");

        if (svc_doc) yjson_doc_free(svc_doc);
        free(traces_json); free(services_json); free(stats_json);
    }

    buf_puts(&b, "</main></body></html>");
    send_response(s, 200, "text/html; charset=utf-8", b.data ? b.data : "", b.len);
    buf_free(&b);

    free(service); free(status); free(since_s); free(trace);
}

static void serve_one(struct yloop *loop, struct yloop_stream *s, void *ud)
{
    const struct app_config *cfg = ud;
    char *buf = malloc(REQ_BUF);
    if (!buf) { yloop_close(s); return; }
    size_t total = 0, last = 0;
    const char *method = NULL, *path = NULL;
    size_t method_len = 0, path_len = 0;
    int minor = 0;
    struct phr_header hdrs[MAX_HEADERS];
    size_t nhdrs = 0;
    int parsed = -1;
    while (total < REQ_BUF) {
        size_t got = yloop_read_some(s, buf + total, REQ_BUF - total);
        if (got == 0) { free(buf); yloop_close(s); return; }
        total += got;
        nhdrs = MAX_HEADERS;
        parsed = phr_parse_request(buf, total, &method, &method_len, &path, &path_len,
                                   &minor, hdrs, &nhdrs, last);
        if (parsed > 0) break;
        if (parsed == -1) { free(buf); yloop_close(s); return; }
        last = total;
    }
    (void)minor; (void)hdrs; (void)nhdrs;
    if (parsed <= 0 || !(method_len == 3 && memcmp(method, "GET", 3) == 0)) {
        send_response(s, 404, "text/plain; charset=utf-8", "not found\n", 10);
        free(buf); yloop_close(s); return;
    }
    char full_path[2048];
    size_t copy = path_len < sizeof(full_path) - 1 ? path_len : sizeof(full_path) - 1;
    memcpy(full_path, path, copy); full_path[copy] = 0;
    if (full_path[0] == '/' && (full_path[1] == 0 || full_path[1] == '?')) {
        render_page(s, loop, cfg, full_path);
    } else if (strcmp(full_path, "/echarts.min.js") == 0) {
        /* Vendored ECharts, embedded in the binary — served locally so the
         * trace UI works with no CDN and in an airgapped environment. */
        send_response(s, 200, "application/javascript; charset=utf-8",
                      picotrace_echarts_js, (size_t)picotrace_echarts_js_len);
    } else {
        send_response(s, 404, "text/plain; charset=utf-8", "not found\n", 10);
    }
    free(buf);
    yloop_close(s);
}

static const struct yargv_option_def OPTIONS[] = {
    {"--upstream-service", NULL, "upstream_service", "Mesh upstream service name (default trace_collector)", YARGV_VALUE, 0},
    {"--upstream-host", NULL, "upstream_host", "Resolved upstream host (default 127.0.0.1)", YARGV_VALUE, 0},
    {"--upstream-port", NULL, "upstream_port", "Resolved upstream port", YARGV_VALUE, 0},
    {"--host", NULL, "host", "Bind address (default 127.0.0.1)", YARGV_VALUE, 0},
    {"--port", NULL, "port", "Bind port (default 8232)", YARGV_VALUE, 0},
    {"--verbose", "-v", "verbose", "Enable debug logging", YARGV_BOOL, 0},
    {"--help", "-h", "help", "Show usage", YARGV_BOOL, 0},
};

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [--upstream-service trace_collector] [--upstream-host 127.0.0.1] [--upstream-port PORT] [--host 127.0.0.1] [--port 8232]\n", prog);
}

int main(int argc, char **argv)
{
    setbuf(stdout, NULL); setbuf(stderr, NULL);

    /* Ignore SIGPIPE process-wide. A write to a peer that has closed its end
     * must surface as EPIPE on the syscall, not kill the process. The upstream
     * collector connection (a raw blocking fd) and the client response writes
     * both go to short-lived peers; under load the collector drops a query
     * mid-call, and without this the next write would terminate picotrace.
     * The mesh-hosted services get this from yengine; this standalone app does
     * not go through yengine, so it must set it itself. */
    signal(SIGPIPE, SIG_IGN);

    struct yargv_chain_ptr_result pr = yargv_parse(OPTIONS, sizeof(OPTIONS) / sizeof(OPTIONS[0]), argc, argv);
    if (PICOMESH_IS_ERR(pr)) {
        fprintf(stderr, "picotrace: argv parse: %s\n", pr.error.msg ? pr.error.msg : "?");
        picomesh_error_destroy(pr.error);
        return 2;
    }
    struct yargv_chain *cli = pr.value;
    if (yargv_get_bool(cli, "help", 0)) { usage(argv[0]); yargv_chain_destroy(cli); return 0; }
    if (yargv_get_bool(cli, "verbose", 0)) ytrace_set_all_enabled(true);

    const char *host = yargv_get_string(cli, "host", "127.0.0.1");
    int port = (int)yargv_get_int(cli, "port", 8232);

    struct app_config cfg = {
        .upstream_service = yargv_get_string(cli, "upstream_service", "trace_collector"),
        .upstream_host = yargv_get_string(cli, "upstream_host", "127.0.0.1"),
        .upstream_port = (int)yargv_get_int(cli, "upstream_port", 0),
    };
    if (cfg.upstream_port <= 0) {
        fprintf(stderr, "picotrace: --upstream-port is required when running outside mesh reconciliation\n");
        yargv_chain_destroy(cli);
        return 1;
    }
    struct yloop_ptr_result lr = yloop_create();
    if (PICOMESH_IS_ERR(lr)) {
        fprintf(stderr, "picotrace: yloop_create: %s\n", lr.error.msg ? lr.error.msg : "?");
        picomesh_error_destroy(lr.error);
        yargv_chain_destroy(cli);
        return 1;
    }
    struct yloop *loop = lr.value;
    struct picomesh_void_result ls = yloop_listen_tcp(loop, host, port, serve_one, &cfg);
    if (PICOMESH_IS_ERR(ls)) {
        fprintf(stderr, "picotrace: listen: %s\n", ls.error.msg ? ls.error.msg : "?");
        picomesh_error_destroy(ls.error);
        yloop_destroy(loop);
        yargv_chain_destroy(cli);
        return 1;
    }
    yinfo("picotrace: listening on %s:%d (upstream=%s %s:%d)", host, port, cfg.upstream_service, cfg.upstream_host, cfg.upstream_port);
    struct picomesh_void_result rr = yloop_run(loop);
    if (PICOMESH_IS_ERR(rr)) picomesh_error_destroy(rr.error);
    yloop_destroy(loop);
    yargv_chain_destroy(cli);
    return 0;
}
