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
"<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"<title>picomesh service console</title>\n"
"<style>\n"
" :root{--bg:#0b1014;--lift:#141a1f;--row:#1e262c;--bd:#364a47;--fg:#e0e5e4;\n"
"       --mut:#9fa7a8;--faint:#556162;--acc:#6ba892;--acc2:#74c5a5;--err:#e06c6c}\n"
" *{box-sizing:border-box}\n"
" body{margin:0;font:14px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--fg);height:100vh;display:flex;flex-direction:column}\n"
" header{padding:9px 16px;border-bottom:1px solid var(--bd);display:flex;gap:10px;align-items:center}\n"
" header h1{font-size:14px;margin:0;color:var(--acc)}\n"
" header .up{color:var(--faint);font-size:12px;margin-left:auto;font-family:ui-monospace,monospace}\n"
" button{background:var(--row);color:var(--fg);border:1px solid var(--bd);border-radius:4px;padding:4px 9px;cursor:pointer;font:inherit}\n"
" button:hover{border-color:var(--acc)}\n"
" main{flex:1;display:flex;min-height:0}\n"
" #tree{width:38%;max-width:460px;overflow:auto;padding:10px 8px;border-right:1px solid var(--bd)}\n"
" #panel{flex:1;overflow:auto;padding:16px 20px}\n"
" .svc>.hd,.cls>.hd{display:flex;gap:6px;align-items:center;cursor:pointer;padding:3px 4px;border-radius:4px}\n"
" .svc>.hd:hover,.cls>.hd:hover{background:var(--lift)}\n"
" .svc>.hd{font-weight:600}\n"
" .cls{margin-left:14px}\n"
" .cls>.hd{color:var(--mut);font-size:13px;font-family:ui-monospace,monospace}\n"
" .tw{width:12px;display:inline-block;color:var(--faint)}\n"
" .badge{font-size:10px;color:var(--faint);border:1px solid var(--bd);border-radius:3px;padding:0 5px}\n"
" .meths{margin-left:26px;display:none}\n"
" .meths.open{display:block}\n"
" .cls.closed>.meths,.svc.closed>.body{display:none}\n"
" .m{display:block;width:100%;text-align:left;border:0;background:none;color:var(--fg);\n"
"    font-family:ui-monospace,monospace;font-size:12.5px;padding:2px 6px;border-radius:4px;cursor:pointer}\n"
" .m:hover{background:var(--lift)}\n"
" .m.sel{background:var(--row);color:var(--acc2)}\n"
" h2.path{font-family:ui-monospace,monospace;color:var(--acc);font-size:16px;margin:0 0 4px;word-break:break-all}\n"
" .sub{color:var(--faint);font-size:12px;margin:0 0 18px}\n"
" .field{margin:0 0 12px;max-width:640px}\n"
" .field label{display:block;font-size:12px;margin-bottom:3px}\n"
" .field .ty{color:var(--faint);font-family:ui-monospace,monospace}\n"
" .field input{width:100%;background:var(--bg);color:var(--fg);border:1px solid var(--bd);\n"
"   border-radius:4px;padding:6px 8px;font:13px ui-monospace,monospace}\n"
" .field input:focus{outline:0;border-color:var(--acc)}\n"
" .actions{margin:16px 0;display:flex;gap:8px}\n"
" .actions .go{background:var(--acc);color:#06120d;border-color:var(--acc);font-weight:600}\n"
" pre{background:#000;border:1px solid var(--bd);border-radius:5px;padding:10px;\n"
"   white-space:pre-wrap;word-break:break-all;max-height:48vh;overflow:auto;margin:0}\n"
" .err{color:var(--err)}\n"
" .hint{color:var(--faint)}\n"
" .rbar{display:flex;align-items:center;gap:10px;margin-bottom:8px}\n"
" .rstatus{font-family:ui-monospace,monospace;font-size:12px;color:var(--mut)}\n"
" .rstatus.err{color:var(--err)}\n"
" .rtog{font-size:11px;padding:2px 7px;margin-left:auto}\n"
" .rbody{max-height:52vh;overflow:auto}\n"
" table.rt{border-collapse:collapse;font-size:13px;width:auto}\n"
" table.rt th,table.rt td{border:1px solid var(--bd);padding:3px 9px;text-align:left;vertical-align:top}\n"
" table.rt thead th{background:var(--row);color:var(--mut);font-weight:600}\n"
" table.rt tbody th{background:var(--lift);color:var(--faint);font-weight:500;font-family:ui-monospace,monospace}\n"
" table.rt td{font-family:ui-monospace,monospace}\n"
" .rt-list{margin:0;padding-left:20px}\n"
" .rt-null{color:var(--faint)}\n"
"</style></head>\n"
"<body>\n"
"<header><h1>picomesh service console</h1>\n"
"<span class=\"hint\">reflected from /_describe</span>\n"
"<span class=\"up\" id=\"up\"></span><button id=\"reload\">reload</button></header>\n"
"<main>\n"
"  <div id=\"tree\">loading...</div>\n"
"  <div id=\"panel\"><p class=\"hint\">pick a method on the left</p></div>\n"
"</main>\n"
"<script>\n"
"const TOKEN=new URLSearchParams(location.search).get('token');\n"
"function authH(h){h=h||{};if(TOKEN)h['Authorization']='Bearer '+TOKEN;return h;}\n"
"const $=(t,c,x)=>{const e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e;};\n"
"async function jget(u){const r=await fetch(u,{headers:authH({})});const t=await r.text();let d;try{d=JSON.parse(t);}catch(e){d=t;}return{ok:r.ok,status:r.status,data:d};}\n"
"async function jrpc(path,args){const r=await fetch('/_rpc',{method:'POST',headers:authH({'Content-Type':'application/json'}),\n"
"  body:JSON.stringify({path,args,kwargs:{}})});const t=await r.text();let d;try{d=JSON.parse(t);}catch(e){d=t;}return{ok:r.ok,status:r.status,data:d};}\n"
"\n"
"// coerce a form string into the JSON type the C method expects\n"
"function coerce(type,v){\n"
"  const t=(type||'').toLowerCase();\n"
"  if(t.includes('bool')) return ['1','true','yes','y','on'].includes(String(v).toLowerCase());\n"
"  if(/\\b(u?int|int\\d+|size_t|long|unsigned)\\b/.test(t)||/int/.test(t)){\n"
"    if(v==='') return 0; const n=parseInt(v,10); return Number.isNaN(n)?v:n;}\n"
"  if(t.includes('float')||t.includes('double')){\n"
"    if(v==='') return 0; const n=parseFloat(v); return Number.isNaN(n)?v:n;}\n"
"  return String(v);                              // char * and everything else\n"
"}\n"
"\n"
"// ---- result renderer: JSON value -> HTML (array-of-objects -> table) ----\n"
"function esc(s){return String(s).replace(/[&<>\"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]));}\n"
"function isObj(v){return v!==null&&typeof v==='object'&&!Array.isArray(v);}\n"
"function renderValue(v){\n"
"  if(v===null||v===undefined) return '<span class=\"rt-null\">null</span>';\n"
"  if(typeof v==='boolean') return v?'true':'false';\n"
"  if(typeof v==='number') return esc(String(v));\n"
"  if(typeof v==='string'){                              // a string that is itself JSON -> render recursively\n"
"    const s=v.trim();\n"
"    if(s.length>1&&(s[0]==='{'||s[0]==='[')){try{const p=JSON.parse(s);if(p&&typeof p==='object')return renderValue(p);}catch(e){}}\n"
"    return esc(v);\n"
"  }\n"
"  if(Array.isArray(v)){\n"
"    if(!v.length) return '<span class=\"rt-null\">[ empty ]</span>';\n"
"    if(v.every(isObj)){                                  // list of records -> table\n"
"      const keys=[...new Set(v.flatMap(o=>Object.keys(o)))];\n"
"      let h='<table class=\"rt\"><thead><tr><th>#</th>'+keys.map(k=>'<th>'+esc(k)+'</th>').join('')+'</tr></thead><tbody>';\n"
"      v.forEach((row,i)=>{h+='<tr><th>'+i+'</th>'+keys.map(k=>'<td>'+(k in row?renderValue(row[k]):'<span class=\"rt-null\">&mdash;</span>')+'</td>').join('')+'</tr>';});\n"
"      return h+'</tbody></table>';\n"
"    }\n"
"    return '<ol class=\"rt-list\">'+v.map(x=>'<li>'+renderValue(x)+'</li>').join('')+'</ol>';\n"
"  }\n"
"  if(isObj(v)){\n"
"    const e=Object.entries(v);\n"
"    if(!e.length) return '<span class=\"rt-null\">{ empty }</span>';\n"
"    return '<table class=\"rt\"><tbody>'+e.map(([k,val])=>'<tr><th>'+esc(k)+'</th><td>'+renderValue(val)+'</td></tr>').join('')+'</tbody></table>';\n"
"  }\n"
"  return esc(String(v));\n"
"}\n"
"function showResult(out,res){\n"
"  out.innerHTML='';\n"
"  const bar=$('div','rbar');\n"
"  bar.appendChild($('span',res.ok?'rstatus':'rstatus err','HTTP '+res.status));\n"
"  const tog=$('button','rtog','raw json');bar.appendChild(tog);\n"
"  // unwrap {result: ...} / {error: ...}\n"
"  let val=res.data;\n"
"  if(val&&typeof val==='object'){ if('result'in val)val=val.result; else if('error'in val)val=val.error; }\n"
"  const tbl=$('div','rbody');tbl.innerHTML=renderValue(val);\n"
"  const raw=$('pre','rbody');raw.style.display='none';\n"
"  raw.textContent=typeof res.data==='string'?res.data:JSON.stringify(res.data,null,2);\n"
"  let showingRaw=false;\n"
"  tog.onclick=()=>{showingRaw=!showingRaw;tbl.style.display=showingRaw?'none':'';raw.style.display=showingRaw?'':'none';tog.textContent=showingRaw?'table':'raw json';};\n"
"  out.appendChild(bar);out.appendChild(tbl);out.appendChild(raw);\n"
"}\n"
"\n"
"let SELECTED=null;\n"
"async function loadTree(){\n"
"  const tree=document.getElementById('tree');\n"
"  const r=await jget('/_describe');\n"
"  if(!r.ok){tree.innerHTML='';tree.appendChild($('div','err','/_describe HTTP '+r.status));return;}\n"
"  const svcs=(r.data&&r.data.services)||[];\n"
"  tree.innerHTML='';\n"
"  if(!svcs.length){tree.appendChild($('div','hint','no active services'));return;}\n"
"  for(const s of svcs){\n"
"    const box=$('div','svc closed');\n"
"    const hd=$('div','hd');hd.appendChild($('span','tw','\\u25B6'));\n"
"    hd.appendChild($('span',null,s.service));hd.appendChild($('span','badge',s.source||'?'));\n"
"    hd.onclick=()=>{box.classList.toggle('closed');hd.querySelector('.tw').textContent=box.classList.contains('closed')?'\\u25B6':'\\u25BC';};\n"
"    box.appendChild(hd);\n"
"    const body=$('div','body');\n"
"    for(const c of (s.classes||[])){\n"
"      const cb=$('div','cls closed');\n"
"      const clsName=c.class.indexOf(s.service+'.')===0?c.class.slice(s.service.length+1):c.class;\n"
"      const ch=$('div','hd');ch.appendChild($('span','tw','\\u25B6'));ch.appendChild($('span',null,clsName));\n"
"      ch.onclick=()=>{cb.classList.toggle('closed');ch.querySelector('.tw').textContent=cb.classList.contains('closed')?'\\u25B6':'\\u25BC';};\n"
"      cb.appendChild(ch);\n"
"      const ms=$('div','meths open');\n"
"      const pre=(c.qname||'')+'_';\n"
"      for(const mq of (c.methods||[])){\n"
"        const verb=mq.indexOf(pre)===0?mq.slice(pre.length):mq;\n"
"        const path=c.class+'.'+verb;\n"
"        const b=$('button','m',verb);b.dataset.path=path;\n"
"        b.onclick=()=>{document.querySelectorAll('.m.sel').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');loadMethod(path);};\n"
"        ms.appendChild(b);\n"
"      }\n"
"      cb.appendChild(ms);body.appendChild(cb);\n"
"    }\n"
"    box.appendChild(body);tree.appendChild(box);\n"
"  }\n"
"  // deep-link / demo: ?expand=1 opens the whole tree, ?path=svc.cls.verb\n"
"  // pre-selects a method (so a single screenshot shows the fields panel).\n"
"  const q=new URLSearchParams(location.search);\n"
"  const wp=q.get('path');\n"
"  if(q.get('expand')==='1'||wp){\n"
"    document.querySelectorAll('.svc,.cls').forEach(x=>x.classList.remove('closed'));\n"
"    document.querySelectorAll('.tw').forEach(t=>t.textContent='\\u25BC');\n"
"  }\n"
"  if(wp){\n"
"    const b=[...document.querySelectorAll('.m')].find(x=>x.dataset.path===wp);\n"
"    if(b)b.classList.add('sel');\n"
"    loadMethod(wp);\n"
"  }\n"
"}\n"
"\n"
"async function loadMethod(path){\n"
"  SELECTED=path;\n"
"  const q=new URLSearchParams(location.search);\n"
"  const panel=document.getElementById('panel');panel.innerHTML='';\n"
"  panel.appendChild(Object.assign($('h2','path',path),{}));\n"
"  const r=await jget('/'+path+'/_describe');\n"
"  if(!r.ok){panel.appendChild($('div','err','describe HTTP '+r.status));return;}\n"
"  const params=(r.data&&r.data.params)||[];\n"
"  panel.appendChild($('p','sub',params.length?(params.length+' parameter'+(params.length>1?'s':'')):'no parameters'));\n"
"  const form=$('form');const inputs=[];\n"
"  for(const p of params){\n"
"    const f=$('div','field');\n"
"    const lab=$('label');lab.appendChild($('span',null,p.name+' '));lab.appendChild($('span','ty',': '+p.type));\n"
"    f.appendChild(lab);\n"
"    const inp=$('input');inp.placeholder=p.type;inp.dataset.type=p.type;inp.dataset.name=p.name;\n"
"    f.appendChild(inp);form.appendChild(f);inputs.push(inp);\n"
"  }\n"
"  const fillRaw=q.get('args');                    // demo deep-link: prefill fields\n"
"  if(fillRaw){try{const vals=JSON.parse(fillRaw);inputs.forEach((i,ix)=>{if(ix<vals.length)i.value=String(vals[ix]);});}catch(e){}}\n"
"  const act=$('div','actions');\n"
"  const go=$('button','go','Invoke');go.type='button';\n"
"  const out=$('div','result');\n"
"  go.onclick=async()=>{\n"
"    const args=inputs.map(i=>coerce(i.dataset.type,i.value));\n"
"    out.innerHTML='<span class=\"hint\">...</span>';\n"
"    const res=await jrpc(path,args);\n"
"    showResult(out,res);\n"
"  };\n"
"  act.appendChild(go);\n"
"  form.appendChild(act);panel.appendChild(form);\n"
"  panel.appendChild($('div','hint','args sent positionally as ['+params.map(p=>p.name).join(', ')+']'));\n"
"  panel.appendChild(out);\n"
"  if(q.get('invoke')==='1')go.click();            // demo deep-link: auto-run\n"
"}\n"
"\n"
"document.getElementById('reload').onclick=loadTree;\n"
"document.getElementById('up').textContent=location.host;\n"
"loadTree();\n"
"</script>\n"
"</body></html>";

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
        "Cache-Control: no-store, no-cache, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
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
