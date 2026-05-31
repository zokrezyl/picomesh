/* picoforge UI helper.
 *
 * Each backend service exposes a JSON-RPC API at
 *   POST http://host:<port>/create  →  {handle}
 *   POST http://host:<port>/invoke  →  {result | error}
 *
 * Service port table mirrors mesh.services.*.port in picoforge.yaml — keep
 * in sync if you ever shuffle them. */
window.SVC = {
  portalloc:              { port: 8201, klass: 'portalloc_store' },
  storage:                { port: 8202, klass: 'storage_sql' },
  session:                { port: 8203, klass: 'session_store' },
  accounts:               { port: 8204, klass: 'accounts_store' },
  password_authn:         { port: 8205, klass: 'password_authn_store' },
  github_authn:           { port: 8206, klass: 'github_authn_store' },
  token_issuer:           { port: 8207, klass: 'token_issuer_store' },
  issues:                 { port: 8208, klass: 'issues_store' },
  git_repo:               { port: 8209, klass: 'git_repo_store' },
  git_pipeline:           { port: 8210, klass: 'git_pipeline_store' },
  personal_access_tokens: { port: 8211, klass: 'personal_access_tokens_store' },
  mesh:                   { port: 8800, klass: 'mesh_store' },
};

/* Each page gets one handle per service — cache them so we don't
 * spam /create on every invoke. */
const _handles = {};

async function svcHandle(svc) {
  if (_handles[svc]) return _handles[svc];
  const s = SVC[svc];
  if (!s) throw new Error('unknown service ' + svc);
  const url = `http://${location.hostname}:${s.port}/create`;
  const r = await fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({class: s.klass}),
  });
  const j = await r.json();
  if (!j.handle) throw new Error(`create ${s.klass} failed: ${JSON.stringify(j)}`);
  _handles[svc] = j.handle;
  return j.handle;
}

async function invoke(svc, method, args) {
  const s = SVC[svc];
  if (!s) throw new Error('unknown service ' + svc);
  const h = await svcHandle(svc);
  const url = `http://${location.hostname}:${s.port}/invoke`;
  const r = await fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({method, handle: h, args}),
  });
  return r.json();
}

/* Best-effort: hit /describe on every backend, return the live set.
 * Used by the index status panel + the admin dashboard. */
async function probeServices() {
  const out = [];
  for (const [name, s] of Object.entries(SVC)) {
    const url = `http://${location.hostname}:${s.port}/describe?class=${s.klass}`;
    try {
      const r = await fetch(url, {signal: AbortSignal.timeout(1500)});
      const j = await r.json();
      out.push({name, port: s.port, klass: s.klass, live: !!j.class,
                methods: j.methods || []});
    } catch (e) {
      out.push({name, port: s.port, klass: s.klass, live: false, methods: []});
    }
  }
  return out;
}

/* Pull the "result" field out of an invoke JSON or null on error.
 * The result is whatever scalar the impl returned (int / string). */
function R(j) { return j && 'result' in j ? j.result : null; }

window.svcHandle = svcHandle;
window.invoke = invoke;
window.probeServices = probeServices;
window.R = R;
