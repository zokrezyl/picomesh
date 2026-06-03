# picomesh MessagePack RPC — implementation status

Tracks GitHub issue **#17** (refines **#9**). This is the working status /
handoff document; the canonical wire contract is
[`docs/msgpack-rpc.md`](./msgpack-rpc.md).

**Goal:** let services in any language talk to picomesh, and let picomesh C
call foreign services, over a MessagePack wire — **without FFI / C-ABI
binding**. It is wire RPC (msgpack over TCP): decoupled, cross-process,
language-agnostic. The single source of truth is the codegen model
(`model.yaml`), which is both a codegen **output** (from annotated C) and a
hand-authorable **input** (an IDL).

## Pipeline — one source of truth

```
annotated C (calc.c, [[clang::annotate]])
   │  src/picomesh/yclass/gen/codegen.py   (clang AST)
   ▼
model.yaml   ← language-neutral IR, regenerated every build
   │                                         \
   │  codegen.py (C glue)                      \  tools/msgpack-codegen/gen.py
   ▼                                            ▼
C skel + C client (rpc/methods.gen.c)         foreign-language CLIENTS
                                              (python / go / rust / cpp / lua)
```

`model.yaml` is the interchange. The C codegen needs clang; the polyglot
generators need only the YAML — no clang, no C sources.

---

## Implemented

| Part | What | Status |
|---|---|---|
| 0 | Shared active-service resolver + gate | ✅ verified |
| 1a | Vendored cmp codec + adapter | ✅ builds |
| 1b | Codegen `minvoke` (inbound dispatch) | ✅ builds |
| 1c | Inbound `msgpack` frontend | ✅ verified |
| 1d | Reference client + inbound smoke | ✅ **9/9** |
| 2 | Outbound transport, C **caller** side | ✅ **3/3** |
| 3 | Polyglot **client** generators + docs | ✅ py e2e, go/cpp compile |

### Part 0 — shared active-service resolver

- `include/picomesh/yengine/resolve.h`, `src/picomesh/yengine/resolve.c`
  - `picomesh_resolve_service_call(engine, "service.class.method")` — gates:
    the service must be an activated plugin **or** a configured remote on this
    node (registration == activation) **before** any class/method lookup; never
    reaches a global method table ungated. Returns `{ctx, obj (acquired),
    class_qname, method_qname, params}`.
  - `picomesh_service_call_release(...)`.
- `src/picomesh/frontends/yhttp/frontend.c` — `route_json_rpc()` refactored
  onto the resolver (dropped the local `path_to_qnames` + raw object-create +
  `jinvoke_for`). This **also closed the pre-existing global-exposure hole** in
  the gateway `/_rpc`.
- **Verified:** collocated `yhttp.serve_app` + calculator on `:8099` —
  `calculator.calc.add(2,3)` → 200 `{result:5}`; `nope.calc.add` → 404
  `service_not_active`; malformed → 400; unknown method → 404.

### Part 1a — vendored codec

- `vendor/cmp/{cmp.c,cmp.h,LICENSE}` — cmp (MIT, single-file, zero-alloc).
- `include/picomesh/msgpack/msgpack.h`, `src/picomesh/msgpack/msgpack.c` —
  fixed-buffer adapter over cmp (`picomesh_msgpack_{reader,writer}_init`).
- CMake: `cmp.c` + `msgpack.c` in `picomesh_runtime`; `vendor/cmp` as a SYSTEM
  include; `cmp.c` compiled `-w`. The native `yrpc` path is untouched.

### Part 1b — codegen minvoke (inbound dispatch)

- `include/picomesh/yclass/minvoke.h`, `src/picomesh/yclass/minvoke.c` —
  `minvoke_fn` / `minvoke_for` / `minvoke_add_lookup`, the msgpack twin of
  jinvoke.
- `src/picomesh/yclass/gen/codegen.py`: `emit_minvoke` / `emit_munpack_arg` /
  `emit_minvoke_write_result` / `emit_minvoke_table` — per method, decode
  positional msgpack args (with per-type width/signedness/range checks) → call
  the public stub → encode the result. Emitted into every plugin's
  `rpc.gen.c`.

### Part 1c — inbound msgpack frontend

- `include/picomesh/frontends/msgpack/msgpack.h`,
  `src/picomesh/frontends/msgpack/msgpack.c` — `--frontend msgpack` (default
  port 7900). Strict serial, `u32` big-endian length prefix, 1 MiB cap.
  Decodes the `{v,op,path,args,kwargs,headers}` envelope, resolves via
  `picomesh_resolve_service_call`, dispatches via `minvoke_for`, encodes
  `{v,ok,result|error}`. Implements `op=invoke` + a minimal `op=describe`.
- `ymain.c`: `--frontend msgpack` wired into the serve dispatch + validation +
  usage.

### Part 1d — reference client + inbound smoke

- `tools/msgpack-client/picomesh_msgpack.py` — reference client.
- `tools/picoforge/msgpack-smoke.sh` — **9/9**: add/mul, gate
  (`service_not_active`), `no_such_method`, `kwargs_unsupported`, bad-arg-type
  (`call_error`), `frame_too_large`, `describe`. Python's **reference msgpack
  lib** round-trips against our cmp codec.

### Part 2 — outbound transport (C → foreign), caller side

- `src/picomesh/yclass/rpc.h` (decls); `src/picomesh/yclass/rpc.c`:
  `RPC_MODE_MSGPACK`; `peer_channel_create_msgpack(fd)`;
  `peer_channel_is_msgpack(s)`; `peer_channel_msgpack_call(...)` — wraps a
  pre-encoded args array in the envelope, big-endian length-frames it, blocking
  round-trip, returns the response's `result` value bytes.
- `src/picomesh/yclass/gen/codegen.py`: `emit_msgpack_remote_branch` /
  `emit_mpack_write_arg` / `emit_mpack_read_result` / `dotted_path` — when
  `ctx->peer` is a msgpack channel, the `methods.gen.c` stub encodes args,
  calls `peer_channel_msgpack_call`, decodes the result.
- `ymain.c`: `client --transport msgpack`.
- `tools/msgpack-client/echo_server.py` — **hand-written** foreign service.
- `tools/picoforge/msgpack-outbound-smoke.sh` — **3/3**: picomesh C → foreign
  Python service: `6+7=13`, `6*7=42`.

### Part 3 — polyglot client generators + docs

- `docs/msgpack-rpc.md` — wire contract (framing, envelope, type map, error
  codes, kwargs).
- `tools/msgpack-codegen/`:
  - `model.py` — load `model.yaml` → normalized IR (dotted path, arg kinds, ret
    kind); C-type → neutral-kind maps.
  - `gen.py` — CLI: `gen.py --lang <L> --out DIR --module NAME model.yaml...`.
  - `emit_python.py emit_go.py emit_rust.py emit_cpp.py emit_lua.py`.
- `bindings/{python,go,rust,cpp,lua}/calculator_client.*` — generated demo.
- **Verified:** python client end-to-end vs a live frontend (add 5 / mul 42 /
  sub 6); go parses (`gofmt -e`); c++ compiles vs cmp (`g++ -fsyntax-only`).
  rust + lua are generate-only here (rustc libz broken, lua absent).

---

## Wire contract (summary)

```
request : u32 BE len | msgpack({v, op, path, args, kwargs, headers})
reply   : u32 BE len | msgpack({v, ok:true,  result: <value>})
                     | msgpack({v, ok:false, error: {message, code}})
```

Picomesh envelope, **not** standard msgpack-rpc. Length-prefixed, strict
serial, 1 MiB cap. `kwargs` is positional-only in v1 (non-empty →
`kwargs_unsupported`). Full detail in [`docs/msgpack-rpc.md`](./msgpack-rpc.md).

## Verify

```sh
make build-desktop-release
bash tools/picoforge/msgpack-smoke.sh            # inbound  9/9
bash tools/picoforge/msgpack-outbound-smoke.sh   # outbound 3/3
# polyglot: tools/msgpack-codegen/gen.py per lang, then run the python client
```

---

## Missing / deferred

1. **Foreign server-skeleton generators — the biggest gap.** The reverse
   direction's *server* half is hand-written today
   (`tools/msgpack-client/echo_server.py`). Need `emit_python_server` /
   `emit_go_server` / … : from `model.yaml`, emit decode → dispatch-on-path →
   encode with typed handler stubs the developer fills in. Then "C calls a
   foreign implementation" is generated end-to-end. (This was the #17 open
   question — server skeletons vs client stubs only; only **clients** exist.)

2. **C client stubs from a hand-authored `model.yaml` (no-C-class / IDL
   case).** For a foreign service picomesh has no annotated C class for, you
   hand-author `model.yaml` as the IDL. Today `codegen.py` goes annotated-C →
   `model.yaml` → C stubs and needs impl functions; it cannot consume a
   hand-written `model.yaml` to emit C *client* stubs. Need either a
   `model.yaml` → C-client emitter, or a "thin annotated-C interface header"
   convention. (The foreign *client* generators already consume `model.yaml`
   directly, so only the C-client side of this case is missing.)

3. **Engine config `transport: msgpack` for remotes (async integration).**
   Outbound works over a *blocking* peer channel (CLI / worker-pool). The
   engine's remote sessions run on the yloop via an async multiplexing mux
   (`rpc_async_client` in `engine.c`). Selecting msgpack for a config remote
   (`remotes: [{transport: msgpack}]`) needs an async yloop msgpack mux
   mirroring that — a blocking channel must not be dropped onto the event loop.
   So a C *service* on the loop calling out via config isn't wired yet; the
   generated glue itself works over the blocking path.

4. **Build wiring for bindings (automation).** `model.yaml` regenerates with
   the build, but the polyglot clients do **not** — `gen.py` is invoked by
   hand. Need a `make bindings` / CMake target that runs `gen.py` per plugin ×
   language from each `model.yaml` on model change.

5. **Bindings only generated for `calculator` (coverage).** 18 plugins have a
   `model.yaml` (accounts, git_repo, session, storage, …); only calculator
   clients were generated as the verified demo. Generating the rest is just
   looping `gen.py` (no new code).

6. **yttp / cli not on the resolver (scope).** Only the gateway `/_rpc` and the
   msgpack frontend go through `picomesh_resolve_service_call`. yttp / cli are
   local object-handle dispatchers (the caller already holds a handle) — not
   service-path boundaries, so the gate doesn't map to them. Intentional; noted
   for completeness.

7. **`describe` is minimal (inbound).** The msgpack frontend's `op=describe`
   returns a method's positional param signature only — now gated through
   `picomesh_resolve_service_call` (uses `call.params`), same as invoke. No
   service-tree / class enumeration like `/_describe_tree` yet.

8. **Object-handle args not crossed (type surface).** v1 targets flat
   value/string methods. struct / object-handle args are process-local and not
   represented on the msgpack wire (cross-language object proxying is out of
   scope).

---

## Housekeeping

- Nothing committed (no commit without an explicit ask).
- Pre-merge formatter (`qa-tools/refactoring/code-format/apply-format.py`) not
  run yet.
- The frontend is named `msgpack` (like `alpine` / `cli`), **not** `ymsgpack`.
- Decisions: vendored **cmp** (not mpack); **Picomesh envelope** (not standard
  msgpack-rpc); kwargs positional-only.
