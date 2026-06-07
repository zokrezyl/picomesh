#!/usr/bin/env node
/* Run the picoforge riscv64 Linux VM inside Node.js — no qemu.
 *
 * Loads the TinyEMU-compiled-to-wasm module (build-webasm-yemu-release/
 * picomesh-yemu.js), boots the same kernel + opensbi + alpine-rootfs the
 * browser/qemu paths use, and exposes the in-guest services to the HOST as
 * plain HTTP proxies — so you can `curl`/browse them like any local server:
 *
 *     guest :8080 (gateway)  ->  host  http://127.0.0.1:18080
 *     guest :8081 (webapp)   ->  host  http://127.0.0.1:18081
 *
 * The guest has no real host socket under wasm; the bridge exposes a slirp
 * "session" API (tinyemu_session_open/send/close) and pushes guest replies
 * back through window.__yettyTinyemuPostSessionRx. This harness drives that
 * API to turn each host HTTP request into an in-guest TCP session.
 *
 * Usage:   node tools/picoforge/yemu/node/picomesh-vm.cjs
 *          [--build DIR] [--gw-port 18080] [--web-port 18081]
 *          [--quiet]   (suppress the guest console stream)
 *
 * Requires: a built build-webasm-yemu-release/ (make build-webasm-yemu-release).
 */
'use strict';
const fs = require('fs');
const path = require('path');
const http = require('http');

// ---- args ----------------------------------------------------------------
const argv = process.argv.slice(2);
function opt(name, def) {
    const i = argv.indexOf(name);
    return i >= 0 && i + 1 < argv.length ? argv[i + 1] : def;
}
const QUIET = argv.includes('--quiet');
const BUILD = path.resolve(opt('--build',
    path.join(__dirname, '..', '..', '..', '..', 'build-webasm-yemu-release')));
const GW_HOST_PORT = parseInt(opt('--gw-port', '18080'), 10);
const WEB_HOST_PORT = parseInt(opt('--web-port', '18081'), 10);

const JS = path.join(BUILD, 'picomesh-yemu.js');
const WASM = path.join(BUILD, 'picomesh-yemu.wasm');
const ASSETS = {
    '/vm/picomesh.cfg':         path.join(BUILD, 'picomesh.cfg'),
    '/vm/kernel-riscv64.bin':   path.join(BUILD, 'assets', 'kernel-riscv64.bin'),
    '/vm/opensbi-fw_jump.elf':  path.join(BUILD, 'assets', 'opensbi-fw_jump.elf'),
    '/vm/alpine-rootfs.img':    path.join(BUILD, 'assets', 'alpine-rootfs.img'),
};
for (const f of [JS, WASM, ...Object.values(ASSETS)]) {
    if (!fs.existsSync(f)) {
        console.error(`FATAL: missing ${f}\n  run: make build-webasm-yemu-release`);
        process.exit(1);
    }
}

// ---- bridge callbacks (the wasm calls window.__yetty*) --------------------
// Per-session receive buffers: the bridge pushes guest→host bytes here.
const rx = new Map(); // sid -> { chunks: [Buffer], total: number }
globalThis.__yettyTinyemuPostSessionRx = function (sid, ptr, len) {
    if (len <= 0) return;
    const copy = Buffer.from(Module.HEAPU8.subarray(ptr, ptr + len)); // must copy: heap moves
    let s = rx.get(sid);
    if (!s) { s = { chunks: [], total: 0 }; rx.set(sid, s); }
    s.chunks.push(copy);
    s.total += copy.length;
};
// Guest console (kernel boot + every [init]/[mesh]/[webapp]/[rpc]/[probe]
// line + the FATAL crash backtrace) → our stdout, unless --quiet.
globalThis.__yettyTinyemuPostOutput = function (ptr, len) {
    if (QUIET || len <= 0) return;
    process.stdout.write(Buffer.from(Module.HEAPU8.subarray(ptr, ptr + len)));
};

// ---- emscripten Module ---------------------------------------------------
let vmReady = false;
const Module = {
    noInitialRun: false,
    locateFile: (p) => (p.endsWith('.wasm') ? WASM : p),
    print:    (t) => { if (!QUIET) process.stdout.write('[tinyemu] ' + t + '\n'); },
    printErr: (t) => process.stderr.write('[tinyemu] ' + t + '\n'),
    onRuntimeInitialized: function () {
        // Emscripten has finished env-detection (NODE) by now, so it is safe
        // to alias window → the bridge's `window.__yetty*` EM_ASM calls need a
        // `window` object; setting it earlier would make emscripten think it
        // is a browser and reach for document/etc.
        globalThis.window = globalThis;
        try { Module.FS.mkdir('/vm'); } catch (e) { /* EEXIST */ }
        for (const [dest, src] of Object.entries(ASSETS)) {
            const bytes = fs.readFileSync(src);
            Module.FS.writeFile(dest, bytes);
            console.error(`[node-vm] MEMFS ${dest}  (${bytes.length} bytes)`);
        }
        console.error('[node-vm] booting RISC-V Linux (tinyemu.wasm)…');
        const rc = Module._tinyemu_bridge_start();
        console.error('[node-vm] tinyemu_bridge_start rc=' + rc);
        if (rc === 0) vmReady = true;
        else { console.error('[node-vm] VM init FAILED'); process.exit(1); }
    },
};
// The emscripten glue declares its own module-scoped `var Module = typeof
// Module != 'undefined' ? Module : {}`. Under require() that hoisted local
// shadows globalThis.Module, so our config (onRuntimeInitialized, locateFile,
// …) would be ignored. Inject it into the SAME scope: write a tiny wrapper
// module that assigns `Module = <our config>` before the (concatenated)
// emscripten source runs — the later `var Module` then keeps our object. The
// wrapper is a normal CJS module, so require/__dirname/process still work.
globalThis.__PF_MODULE_CONFIG = Module;

// ---- one in-guest HTTP round-trip via the slirp session API --------------
// Opens a TCP session to (guest, port), sends the raw request, then collects
// the reply using the same FIN-via-quiet-time heuristic the browser uses
// (the request sends `Connection: close`, so the guest closes when done).
function vmRequest(port, rawReq, { firstByteTimeoutMs = 60000, quietTicks = 4, tickMs = 100 } = {}) {
    return new Promise((resolve, reject) => {
        const sid = Module._tinyemu_session_open(port);
        if (sid < 0) return reject(new Error('tinyemu_session_open(' + port + ') failed'));
        rx.set(sid, { chunks: [], total: 0 });
        const p = Module._malloc(rawReq.length);
        Module.HEAPU8.set(rawReq, p);
        Module._tinyemu_session_send(sid, p, rawReq.length);
        Module._free(p);

        const t0 = Date.now();
        let lastTotal = 0, quiet = 0;
        const timer = setInterval(() => {
            const s = rx.get(sid);
            if (s.total > lastTotal) { lastTotal = s.total; quiet = 0; return; }
            if (s.total === 0) {
                if (Date.now() - t0 < firstByteTimeoutMs) return; // still waiting for first byte
            } else if (++quiet < quietTicks) {
                return; // wait for the response to go quiet (≈ FIN)
            }
            clearInterval(timer);
            try { Module._tinyemu_session_close(sid); } catch (e) { /* ignore */ }
            const out = Buffer.concat(s.chunks, s.total);
            rx.delete(sid);
            if (out.length === 0) return reject(new Error('no response from guest:' + port + ' (service up yet?)'));
            resolve(out);
        }, tickMs);
    });
}

// Relay a raw guest HTTP response (status line + headers + body) to the host
// client, preserving status, headers (incl. multiple Set-Cookie), and body.
function relayResponse(res, raw) {
    const sep = raw.indexOf('\r\n\r\n');
    if (sep < 0) { res.socket.end(raw); return; }
    const headText = raw.slice(0, sep).toString('latin1');
    const body = raw.slice(sep + 4);
    const lines = headText.split('\r\n');
    const m = /^HTTP\/\d\.\d\s+(\d+)/.exec(lines[0] || '');
    const status = m ? parseInt(m[1], 10) : 502;
    const headers = {};
    for (let i = 1; i < lines.length; i++) {
        const c = lines[i].indexOf(':');
        if (c < 0) continue;
        const k = lines[i].slice(0, c).trim();
        const v = lines[i].slice(c + 1).trim();
        const kl = k.toLowerCase();
        if (kl === 'connection' || kl === 'transfer-encoding' || kl === 'content-length') continue;
        if (headers[k] === undefined) headers[k] = v;
        else if (Array.isArray(headers[k])) headers[k].push(v);
        else headers[k] = [headers[k], v];
    }
    res.writeHead(status, headers);
    res.end(body);
}

// ---- host HTTP proxy → guest port ----------------------------------------
function startProxy(hostPort, guestPort, label) {
    const srv = http.createServer((req, res) => {
        const chunks = [];
        req.on('data', (c) => chunks.push(c));
        req.on('end', async () => {
            if (!vmReady) { res.writeHead(503); res.end('VM still booting…\n'); return; }
            const body = Buffer.concat(chunks);
            let head = `${req.method} ${req.url} HTTP/1.1\r\nHost: picomesh-yemu\r\n`;
            for (let i = 0; i < req.rawHeaders.length; i += 2) {
                const k = req.rawHeaders[i], kl = k.toLowerCase();
                if (kl === 'host' || kl === 'connection' || kl === 'content-length') continue;
                head += `${k}: ${req.rawHeaders[i + 1]}\r\n`;
            }
            head += `Content-Length: ${body.length}\r\nConnection: close\r\n\r\n`;
            const raw = Buffer.concat([Buffer.from(head, 'utf8'), body]);
            try {
                relayResponse(res, await vmRequest(guestPort, raw));
            } catch (e) {
                res.writeHead(502, { 'content-type': 'text/plain' });
                res.end('node-vm proxy error: ' + e.message + '\n');
            }
        });
    });
    srv.listen(hostPort, '127.0.0.1', () =>
        console.error(`[node-vm] proxy http://127.0.0.1:${hostPort}  ->  guest:${guestPort}  (${label})`));
}

startProxy(GW_HOST_PORT, 8080, 'gateway API');
startProxy(WEB_HOST_PORT, 8081, 'webapp');

console.error(`[node-vm] loading ${path.basename(JS)} …`);
const os = require('os');
const wrapped = path.join(os.tmpdir(), `picomesh-yemu-node.${process.pid}.cjs`);
fs.writeFileSync(wrapped, 'Module = globalThis.__PF_MODULE_CONFIG;\n' + fs.readFileSync(JS, 'utf8'));
process.on('exit', () => { try { fs.unlinkSync(wrapped); } catch (e) { /* ignore */ } });
require(wrapped); // boots the VM; onRuntimeInitialized fires when the runtime is up
