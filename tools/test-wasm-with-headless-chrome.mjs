// CDP driver for tools/test-wasm-with-headless-chrome.sh
//
// Spawns Chrome (inheriting the xvfb DISPLAY the wrapper set up), points
// it at the wasm demo's tinyemu-iframe.html, then drives the page over
// the Chrome DevTools Protocol the same way a user would:
//
//   boot the in-VM gateway -> register -> create repo -> open repo
//   -> list account repos
//
// Every check asserts on the *rendered HTML* inside the demo iframe's
// srcdoc, not merely on HTTP status, because the gateway happily returns
// 200 with an error body. Prints "PASS"/"FAIL" per check and exits 1 if
// any check fails so CI / the shell wrapper can gate on it.
//
// Requires node >= 18 (global fetch + WebSocket). Tested on node 22.

import { spawn } from 'node:child_process';

const HTTP_PORT   = Number(process.env.HTTP_PORT || 8090);
const BOOT_WAIT_S = Number(process.env.BOOT_WAIT_S || 300);
const DBG         = 9222;
const PAGE = `http://127.0.0.1:${HTTP_PORT}/tinyemu-iframe.html?ptyId=1&cols=120&rows=32`;

const T0 = Date.now();
const log = (...a) => console.log('[t+' + ((Date.now() - T0) / 1000 | 0) + 's]', ...a);
const sleep = ms => new Promise(r => setTimeout(r, ms));

// --- results ------------------------------------------------------------
const checks = [];
function check(name, ok, detail) {
  checks.push({ name, ok: !!ok });
  console.log(`   ${ok ? 'PASS' : 'FAIL'}: ${name}${detail ? '  — ' + detail : ''}`);
}

// --- Chrome under the (xvfb) display the wrapper provides ---------------
const chrome = spawn('google-chrome', [
  '--no-sandbox', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
  '--window-size=1400,1000',
  // Keep rAF / timers running at full rate: the wasm VM only advances on
  // animation frames, and a throttled tab never boots the guest.
  '--disable-background-timer-throttling',
  '--disable-backgrounding-occluded-windows',
  '--disable-renderer-backgrounding',
  '--disable-features=CalculateNativeWinOcclusion,IntensiveWakeUpThrottling',
  `--remote-debugging-port=${DBG}`,
  '--user-data-dir=/tmp/test-wasm-chrome-profile',
  PAGE,
], { stdio: 'ignore' });

// --- minimal CDP client -------------------------------------------------
async function targetWs() {
  for (let i = 0; i < 60; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${DBG}/json`)).json();
      const p = list.find(t => t.type === 'page' && t.url.includes('tinyemu-iframe'));
      if (p?.webSocketDebuggerUrl) return p.webSocketDebuggerUrl;
    } catch { /* chrome not ready yet */ }
    await sleep(500);
  }
  throw new Error('no CDP target (chrome did not come up)');
}
function cdp(wsUrl) {
  const ws = new WebSocket(wsUrl);
  let id = 0;
  const pend = new Map();
  const ready = new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pend.has(m.id)) { pend.get(m.id)(m); pend.delete(m.id); }
  };
  const send = (method, params = {}) => new Promise(res => {
    const i = ++id; pend.set(i, res);
    ws.send(JSON.stringify({ id: i, method, params }));
  });
  return { ready, send };
}
async function ev(c, expr) {
  const r = await c.send('Runtime.evaluate',
    { expression: `(()=>{ ${expr} })()`, returnByValue: true });
  if (r.result?.exceptionDetails) return { __err: JSON.stringify(r.result.exceptionDetails).slice(0, 200) };
  return r.result?.result?.value;
}

// Netlog rows the demo records for every proxied request.
const NETLOG = `return [...document.querySelectorAll('#netlog-tbody tr')].map(tr=>(
  (tr.querySelector('.col-method')||{}).textContent+' '+
  (tr.querySelector('.col-path')||{}).textContent+' -> '+
  (tr.querySelector('.col-status')||{}).textContent));`;

// The HTML the demo iframe is currently showing (it renders gateway
// responses into the inner iframe's srcdoc).
const SRCDOC = `var f=document.getElementById('svc-iframe');return f?(f.srcdoc||''):'(no iframe)';`;

async function netlog(c) { return (await ev(c, NETLOG)) || []; }
async function srcdoc(c) { return String((await ev(c, SRCDOC)) || ''); }

// Post a picomesh-submit/picomesh-nav message and wait until `pathRe` shows a
// numeric status in the netlog (or timeout). Returns the matching row.
async function driveAndWait(c, message, pathRe, timeoutS) {
  await ev(c, `window.postMessage(${JSON.stringify(message)}, '*'); return 1;`);
  for (let i = 0; i < timeoutS / 3; i++) {
    await sleep(3000);
    const rows = await netlog(c);
    const row = rows.filter(x => pathRe.test(x)).slice(-1)[0] || '';
    if (/-> [0-9]/.test(row)) return row;
  }
  return '';
}

let exitCode = 0;
try {
  const c = cdp(await targetWs());
  await c.ready;
  await c.send('Runtime.enable');

  log('waiting for Module + svc-form…');
  let mod = false;
  for (let i = 0; i < 120; i++) {
    const r = await ev(c, `return (typeof Module!=='undefined'
      && !!(Module && Module._tinyemu_session_open)
      && !!document.getElementById('svc-form'));`);
    if (r === true) { mod = true; break; }
    await sleep(1000);
  }
  check('demo page + wasm Module loaded', mod);
  if (!mod) throw new Error('Module/svc-form never appeared');

  // The HTML page tier is picoforge-webapp on :8081 (GitLab-style /-/ routes);
  // the gateway on :8080 is API-only and 404s GET pages. Drive the webapp.
  log('booting in-VM webapp — re-issuing GET /-/login until 200 (up to ' + BOOT_WAIT_S + 's)…');
  let up = false;
  for (let i = 0; i < BOOT_WAIT_S / 5; i++) {
    await ev(c, `document.getElementById('svc-port').value='8081';
      document.getElementById('svc-path').value='/-/login';
      document.getElementById('svc-form').dispatchEvent(new Event('submit',{cancelable:true}));
      return 1;`);
    await sleep(5000);
    if ((await netlog(c)).some(r => /-\/login -> 200/.test(r))) { up = true; break; }
    if (i % 3 === 0) log('  …still booting; last netlog:', ((await netlog(c)).slice(-1)[0] || '(none)'));
  }
  check('in-VM webapp boots and serves GET /-/login -> 200', up);
  if (!up) throw new Error('gateway never came up');
  log('GATEWAY UP');

  // --- boot smoke: the VM's run.sh registers `demo` and creates repos
  //     hello + world at startup (in the background). Verify that landed:
  //     log in as demo/demo, then poll /demo until both repos are listed.
  //     Each git init is ~tens of seconds under the emulator, so allow a
  //     generous window. This proves create-repo works *inside the VM*.
  log('BOOT SMOKE: logging in as demo/demo, polling /demo for hello + world (up to 300s)');
  await driveAndWait(c,
    { type: 'picomesh-submit', method: 'POST', path: '/-/login', body: 'username=demo&password=demo' },
    /\/(login|demo)/, 30);
  let smokeOk = false;
  // The probe seeds demo/hello + demo/world by creating real git repos inside
  // the guest; git_repository_init under the emulated RISC-V CPU in a browser
  // tab is slow (node-vm does it in seconds, the browser needs much longer),
  // so allow a generous window — this is throughput, not correctness.
  for (let i = 0; i < 60; i++) {
    await driveAndWait(c, { type: 'picomesh-nav', path: '/demo' }, /\/demo\b/, 20);
    const dsd = await srcdoc(c);
    if (dsd.includes('/demo/hello') && dsd.includes('/demo/world')) { smokeOk = true; break; }
    await sleep(5000);
  }
  check('boot smoke populated demo account with repos hello + world', smokeOk,
    smokeOk ? '' : 'demo/hello + demo/world did not both appear within the window');

  // Unique account per run so re-runs don't collide on "already taken".
  const u = 'zoe' + (Date.now() % 100000);

  // --- register ---------------------------------------------------------
  log('REGISTER ' + u);
  const regRow = await driveAndWait(c,
    { type: 'picomesh-submit', method: 'POST', path: '/-/register', body: `username=${u}&password=pw` },
    /\/register/, 30);
  check('POST /register returns a status', /-> [0-9]/.test(regRow), regRow || '(no row)');

  // Account landing renders the namespace header (GitLab-style namespace
  // page). The webapp shows the bare name in an <h1>; accept the @-prefixed
  // form too for forward-compat.
  await driveAndWait(c, { type: 'picomesh-nav', path: '/' + u }, new RegExp('\\/' + u + ' '), 30);
  let sd = await srcdoc(c);
  const acctHdr = sd.includes('<h1>' + u + '</h1>') || sd.includes('@' + u);
  check('account page renders the ' + u + ' namespace header', acctHdr,
    acctHdr ? '' : 'page did not show the account header');

  // --- create repo ------------------------------------------------------
  // git_repository_init is offloaded to the libuv worker pool, so the
  // create request still takes as long as the (emulated) init costs, but
  // the event loop is NOT frozen meanwhile. Fire create WITHOUT awaiting,
  // then fire a concurrent probe GET on a unique path: if the loop were
  // blocked by the init, that probe would stall behind it (~30s); with
  // the offload it answers promptly.
  log('CREATE REPO demo1 (git init offloaded to worker pool — loop must stay responsive)');
  await ev(c, `window.postMessage({type:'picomesh-submit',method:'POST',path:'/-/repos/new',body:'name=demo1'},'*'); return 1;`);
  await sleep(1500);
  // Probe a real served path (a query string on a command path 404s — the
  // route table matches the exact path). /-/login returns 200 fast iff the
  // loop is not frozen behind the offloaded git init.
  const probeRow = await driveAndWait(c, { type: 'picomesh-nav', path: '/-/login' }, /\/-\/login\b/, 12);
  const createRows0 = (await netlog(c)).filter(x => /\/repos\/new/.test(x));
  const createPending = createRows0.length > 0 && !/-> [0-9]/.test(createRows0.slice(-1)[0]);
  check('server stays responsive during slow create (loop not frozen)',
    /-> 200/.test(probeRow),
    createPending ? 'probe answered while create still in flight' : 'probe answered (create already finished)');

  // Now wait for the create itself to land its 303.
  let repoRow = '';
  for (let i = 0; i < 30; i++) {
    const rows = (await netlog(c)).filter(x => /\/repos\/new/.test(x));
    repoRow = rows.slice(-1)[0] || '';
    if (/-> [0-9]/.test(repoRow)) break;
    await sleep(3000);
  }
  const created303 = /-> 303/.test(repoRow);
  check('POST /repos/new -> 303 (repo created)', created303, repoRow || '(no row / timed out)');

  // --- get repo (the repo SHOW page must render, not "repo not found") --
  // GitLab-style routing: the repo browser lives at /<u>/demo1/-/tree (a bare
  // /<u>/demo1 is a NAMESPACE path). The page header is the repo name.
  log('GET REPO ' + u + '/demo1/-/tree');
  await driveAndWait(c, { type: 'picomesh-nav', path: `/${u}/demo1/-/tree` }, new RegExp(`${u}\\/demo1`), 60);
  sd = await srcdoc(c);
  const notFound = /repo not found/i.test(sd);
  const repoShown = sd.includes('<h1>demo1</h1>') && !notFound;
  check('GET /' + u + '/demo1/-/tree renders the repo page', repoShown,
    notFound ? 'page said "repo not found"' : (repoShown ? '' : 'repo-show marker missing'));

  // --- list account repos ----------------------------------------------
  log('LIST repos on /' + u);
  await driveAndWait(c, { type: 'picomesh-nav', path: '/' + u }, new RegExp('\\/' + u + ' '), 30);
  sd = await srcdoc(c);
  const listed = sd.includes(`/${u}/demo1`);
  check('account page lists demo1', listed,
    listed ? '' : 'no link to /' + u + '/demo1 on the account page');

  log('=== final NETLOG ===');
  (await netlog(c)).forEach(r => console.log('   | ' + r));

} catch (e) {
  check('driver completed without throwing', false, e.message);
} finally {
  chrome.kill('SIGKILL');
}

const passed = checks.filter(c => c.ok).length;
console.log(`\nchecks: ${passed}/${checks.length} passed`);
exitCode = checks.length > 0 && checks.every(c => c.ok) ? 0 : 1;
process.exit(exitCode);
