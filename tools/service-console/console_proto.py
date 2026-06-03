#!/usr/bin/env python3
"""Generic service-console PROTOTYPE (gh#15).

A standalone, app-agnostic console that reflects a picomesh service tree
PURELY through the runtime `/_describe` API and invokes through JSON
`/_rpc`. It reads NO build artifacts (no model.yaml) — everything it shows
comes from the live endpoint, exactly as it would in a deployment image:

    GET  /_describe                       -> services + classes + method names
    GET  /<service.class.method>/_describe -> that method's parameter fields
    POST /_rpc {"path","args","kwargs"}    -> invoke

This is a throwaway Python prototype of the UX (left: tree of
services -> classes -> methods; right: one labelled input per parameter,
e.g. sharded_storage.db.set -> namespace/key/value). It points at any
yhttp-compatible endpoint and proxies the browser's calls there so the
page is same-origin.

    ./console_proto.py --upstream http://127.0.0.1:8230 --port 8232
    open http://127.0.0.1:8232/_alpine

stdlib only — no dependencies.
"""

import argparse
import json
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>picomesh service console (proto)</title>
<style>
 :root{--bg:#0b1014;--lift:#141a1f;--row:#1e262c;--bd:#364a47;--fg:#e0e5e4;
       --mut:#9fa7a8;--faint:#556162;--acc:#6ba892;--acc2:#74c5a5;--err:#e06c6c}
 *{box-sizing:border-box}
 body{margin:0;font:14px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--fg);height:100vh;display:flex;flex-direction:column}
 header{padding:9px 16px;border-bottom:1px solid var(--bd);display:flex;gap:10px;align-items:center}
 header h1{font-size:14px;margin:0;color:var(--acc)}
 header .up{color:var(--faint);font-size:12px;margin-left:auto;font-family:ui-monospace,monospace}
 button{background:var(--row);color:var(--fg);border:1px solid var(--bd);border-radius:4px;padding:4px 9px;cursor:pointer;font:inherit}
 button:hover{border-color:var(--acc)}
 main{flex:1;display:flex;min-height:0}
 #tree{width:38%;max-width:460px;overflow:auto;padding:10px 8px;border-right:1px solid var(--bd)}
 #panel{flex:1;overflow:auto;padding:16px 20px}
 .svc>.hd,.cls>.hd{display:flex;gap:6px;align-items:center;cursor:pointer;padding:3px 4px;border-radius:4px}
 .svc>.hd:hover,.cls>.hd:hover{background:var(--lift)}
 .svc>.hd{font-weight:600}
 .cls{margin-left:14px}
 .cls>.hd{color:var(--mut);font-size:13px;font-family:ui-monospace,monospace}
 .tw{width:12px;display:inline-block;color:var(--faint)}
 .badge{font-size:10px;color:var(--faint);border:1px solid var(--bd);border-radius:3px;padding:0 5px}
 .meths{margin-left:26px;display:none}
 .meths.open{display:block}
 .cls.closed>.meths,.svc.closed>.body{display:none}
 .m{display:block;width:100%;text-align:left;border:0;background:none;color:var(--fg);
    font-family:ui-monospace,monospace;font-size:12.5px;padding:2px 6px;border-radius:4px;cursor:pointer}
 .m:hover{background:var(--lift)}
 .m.sel{background:var(--row);color:var(--acc2)}
 h2.path{font-family:ui-monospace,monospace;color:var(--acc);font-size:16px;margin:0 0 4px;word-break:break-all}
 .sub{color:var(--faint);font-size:12px;margin:0 0 18px}
 .field{margin:0 0 12px;max-width:640px}
 .field label{display:block;font-size:12px;margin-bottom:3px}
 .field .ty{color:var(--faint);font-family:ui-monospace,monospace}
 .field input{width:100%;background:var(--bg);color:var(--fg);border:1px solid var(--bd);
   border-radius:4px;padding:6px 8px;font:13px ui-monospace,monospace}
 .field input:focus{outline:0;border-color:var(--acc)}
 .actions{margin:16px 0;display:flex;gap:8px}
 .actions .go{background:var(--acc);color:#06120d;border-color:var(--acc);font-weight:600}
 pre{background:#000;border:1px solid var(--bd);border-radius:5px;padding:10px;
   white-space:pre-wrap;word-break:break-all;max-height:48vh;overflow:auto;margin:0}
 .err{color:var(--err)}
 .hint{color:var(--faint)}
 .rbar{display:flex;align-items:center;gap:10px;margin-bottom:8px}
 .rstatus{font-family:ui-monospace,monospace;font-size:12px;color:var(--mut)}
 .rstatus.err{color:var(--err)}
 .rtog{font-size:11px;padding:2px 7px;margin-left:auto}
 .rbody{max-height:52vh;overflow:auto}
 table.rt{border-collapse:collapse;font-size:13px;width:auto}
 table.rt th,table.rt td{border:1px solid var(--bd);padding:3px 9px;text-align:left;vertical-align:top}
 table.rt thead th{background:var(--row);color:var(--mut);font-weight:600}
 table.rt tbody th{background:var(--lift);color:var(--faint);font-weight:500;font-family:ui-monospace,monospace}
 table.rt td{font-family:ui-monospace,monospace}
 .rt-list{margin:0;padding-left:20px}
 .rt-null{color:var(--faint)}
</style></head>
<body>
<header><h1>picomesh service console</h1>
<span class="hint">reflected from /_describe</span>
<span class="up" id="up"></span><button id="reload">reload</button></header>
<main>
  <div id="tree">loading…</div>
  <div id="panel"><p class="hint">pick a method on the left</p></div>
</main>
<script>
const $=(t,c,x)=>{const e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e;};
async function jget(u){const r=await fetch(u);const t=await r.text();let d;try{d=JSON.parse(t);}catch(e){d=t;}return{ok:r.ok,status:r.status,data:d};}
async function jrpc(path,args){const r=await fetch('/_rpc',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({path,args,kwargs:{}})});const t=await r.text();let d;try{d=JSON.parse(t);}catch(e){d=t;}return{ok:r.ok,status:r.status,data:d};}

// coerce a form string into the JSON type the C method expects
function coerce(type,v){
  const t=(type||'').toLowerCase();
  if(t.includes('bool')) return ['1','true','yes','y','on'].includes(String(v).toLowerCase());
  if(/\b(u?int|int\d+|size_t|long|unsigned)\b/.test(t)||/int/.test(t)){
    if(v==='') return 0; const n=parseInt(v,10); return Number.isNaN(n)?v:n;}
  if(t.includes('float')||t.includes('double')){
    if(v==='') return 0; const n=parseFloat(v); return Number.isNaN(n)?v:n;}
  return String(v);                              // char * and everything else
}

// ---- result renderer: JSON value -> HTML (array-of-objects -> table) ----
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function isObj(v){return v!==null&&typeof v==='object'&&!Array.isArray(v);}
function renderValue(v){
  if(v===null||v===undefined) return '<span class="rt-null">null</span>';
  if(typeof v==='boolean') return v?'true':'false';
  if(typeof v==='number') return esc(String(v));
  if(typeof v==='string'){                              // a string that is itself JSON -> render recursively
    const s=v.trim();
    if(s.length>1&&(s[0]==='{'||s[0]==='[')){try{const p=JSON.parse(s);if(p&&typeof p==='object')return renderValue(p);}catch(e){}}
    return esc(v);
  }
  if(Array.isArray(v)){
    if(!v.length) return '<span class="rt-null">[ empty ]</span>';
    if(v.every(isObj)){                                  // list of records -> table
      const keys=[...new Set(v.flatMap(o=>Object.keys(o)))];
      let h='<table class="rt"><thead><tr><th>#</th>'+keys.map(k=>'<th>'+esc(k)+'</th>').join('')+'</tr></thead><tbody>';
      v.forEach((row,i)=>{h+='<tr><th>'+i+'</th>'+keys.map(k=>'<td>'+(k in row?renderValue(row[k]):'<span class="rt-null">—</span>')+'</td>').join('')+'</tr>';});
      return h+'</tbody></table>';
    }
    return '<ol class="rt-list">'+v.map(x=>'<li>'+renderValue(x)+'</li>').join('')+'</ol>';
  }
  if(isObj(v)){
    const e=Object.entries(v);
    if(!e.length) return '<span class="rt-null">{ empty }</span>';
    return '<table class="rt"><tbody>'+e.map(([k,val])=>'<tr><th>'+esc(k)+'</th><td>'+renderValue(val)+'</td></tr>').join('')+'</tbody></table>';
  }
  return esc(String(v));
}
function showResult(out,res){
  out.innerHTML='';
  const bar=$('div','rbar');
  bar.appendChild($('span',res.ok?'rstatus':'rstatus err','HTTP '+res.status));
  const tog=$('button','rtog','raw json');bar.appendChild(tog);
  // unwrap {result: ...} / {error: ...}
  let val=res.data;
  if(val&&typeof val==='object'){ if('result'in val)val=val.result; else if('error'in val)val=val.error; }
  const tbl=$('div','rbody');tbl.innerHTML=renderValue(val);
  const raw=$('pre','rbody');raw.style.display='none';
  raw.textContent=typeof res.data==='string'?res.data:JSON.stringify(res.data,null,2);
  let showingRaw=false;
  tog.onclick=()=>{showingRaw=!showingRaw;tbl.style.display=showingRaw?'none':'';raw.style.display=showingRaw?'':'none';tog.textContent=showingRaw?'table':'raw json';};
  out.appendChild(bar);out.appendChild(tbl);out.appendChild(raw);
}

let SELECTED=null;
async function loadTree(){
  const tree=document.getElementById('tree');
  const r=await jget('/_describe');
  if(!r.ok){tree.innerHTML='';tree.appendChild($('div','err','/_describe HTTP '+r.status));return;}
  const svcs=(r.data&&r.data.services)||[];
  tree.innerHTML='';
  if(!svcs.length){tree.appendChild($('div','hint','no active services'));return;}
  for(const s of svcs){
    const box=$('div','svc closed');
    const hd=$('div','hd');hd.appendChild($('span','tw','▶'));
    hd.appendChild($('span',null,s.service));hd.appendChild($('span','badge',s.source||'?'));
    hd.onclick=()=>{box.classList.toggle('closed');hd.querySelector('.tw').textContent=box.classList.contains('closed')?'▶':'▼';};
    box.appendChild(hd);
    const body=$('div','body');
    for(const c of (s.classes||[])){
      const cb=$('div','cls closed');
      const clsName=c.class.indexOf(s.service+'.')===0?c.class.slice(s.service.length+1):c.class;
      const ch=$('div','hd');ch.appendChild($('span','tw','▶'));ch.appendChild($('span',null,clsName));
      ch.onclick=()=>{cb.classList.toggle('closed');ch.querySelector('.tw').textContent=cb.classList.contains('closed')?'▶':'▼';};
      cb.appendChild(ch);
      const ms=$('div','meths open');
      const pre=(c.qname||'')+'_';
      for(const mq of (c.methods||[])){
        const verb=mq.indexOf(pre)===0?mq.slice(pre.length):mq;
        const path=c.class+'.'+verb;
        const b=$('button','m',verb);b.dataset.path=path;
        b.onclick=()=>{document.querySelectorAll('.m.sel').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');loadMethod(path);};
        ms.appendChild(b);
      }
      cb.appendChild(ms);body.appendChild(cb);
    }
    box.appendChild(body);tree.appendChild(box);
  }
  // deep-link / demo: ?expand=1 opens the whole tree, ?path=svc.cls.verb
  // pre-selects a method (so a single screenshot shows the fields panel).
  const q=new URLSearchParams(location.search);
  const wp=q.get('path');
  if(q.get('expand')==='1'||wp){
    document.querySelectorAll('.svc,.cls').forEach(x=>x.classList.remove('closed'));
    document.querySelectorAll('.tw').forEach(t=>t.textContent='▼');
  }
  if(wp){
    const b=[...document.querySelectorAll('.m')].find(x=>x.dataset.path===wp);
    if(b)b.classList.add('sel');
    loadMethod(wp);
  }
}

async function loadMethod(path){
  SELECTED=path;
  const q=new URLSearchParams(location.search);
  const panel=document.getElementById('panel');panel.innerHTML='';
  panel.appendChild(Object.assign($('h2','path',path),{}));
  const r=await jget('/'+path+'/_describe');
  if(!r.ok){panel.appendChild($('div','err','describe HTTP '+r.status));return;}
  const params=(r.data&&r.data.params)||[];
  panel.appendChild($('p','sub',params.length?(params.length+' parameter'+(params.length>1?'s':'')):'no parameters'));
  const form=$('form');const inputs=[];
  for(const p of params){
    const f=$('div','field');
    const lab=$('label');lab.appendChild($('span',null,p.name+' '));lab.appendChild($('span','ty',': '+p.type));
    f.appendChild(lab);
    const inp=$('input');inp.placeholder=p.type;inp.dataset.type=p.type;inp.dataset.name=p.name;
    f.appendChild(inp);form.appendChild(f);inputs.push(inp);
  }
  const fillRaw=q.get('args');                    // demo deep-link: prefill fields
  if(fillRaw){try{const vals=JSON.parse(fillRaw);inputs.forEach((i,ix)=>{if(ix<vals.length)i.value=String(vals[ix]);});}catch(e){}}
  const act=$('div','actions');
  const go=$('button','go','Invoke');go.type='button';
  const out=$('div','result');
  go.onclick=async()=>{
    const args=inputs.map(i=>coerce(i.dataset.type,i.value));
    out.innerHTML='<span class="hint">…</span>';
    const res=await jrpc(path,args);
    showResult(out,res);
  };
  act.appendChild(go);
  form.appendChild(act);panel.appendChild(form);
  panel.appendChild($('div','hint','args sent positionally as ['+params.map(p=>p.name).join(', ')+']'));
  panel.appendChild(out);
  if(q.get('invoke')==='1')go.click();            // demo deep-link: auto-run
}

document.getElementById('reload').onclick=loadTree;
document.getElementById('up').textContent=location.host;
loadTree();
</script>
</body></html>"""


class Handler(BaseHTTPRequestHandler):
    upstream = "http://127.0.0.1:8230"

    def log_message(self, *a):  # quiet
        pass

    def _send(self, status, ctype, body):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _proxy(self):
        body = None
        if self.command == "POST":
            n = int(self.headers.get("Content-Length", 0) or 0)
            body = self.rfile.read(n) if n else b""
        req = urllib.request.Request(self.upstream + self.path, data=body, method=self.command)
        ct = self.headers.get("Content-Type")
        if ct:
            req.add_header("Content-Type", ct)
        try:
            with urllib.request.urlopen(req, timeout=15) as r:
                self._send(r.status, r.headers.get("Content-Type", "application/json"), r.read())
        except urllib.error.HTTPError as e:
            self._send(e.code, e.headers.get("Content-Type", "application/json"), e.read())
        except Exception as ex:
            self._send(502, "application/json",
                       json.dumps({"error": f"console-proto: upstream unreachable: {ex}"}))

    def do_GET(self):
        if self.path.split("?", 1)[0] in ("/", "/_alpine"):
            self._send(200, "text/html; charset=utf-8", PAGE)
        else:
            self._proxy()

    def do_POST(self):
        self._proxy()


def main():
    ap = argparse.ArgumentParser(description="picomesh generic service-console prototype")
    ap.add_argument("--upstream", default="http://127.0.0.1:8230",
                    help="yhttp endpoint to reflect (bridge or gateway), default %(default)s")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8232)
    args = ap.parse_args()
    Handler.upstream = args.upstream.rstrip("/")
    srv = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"service console -> {Handler.upstream}", file=sys.stderr)
    print(f"open http://{args.host}:{args.port}/_alpine", file=sys.stderr)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
