#!/usr/bin/env bash
# Build picomesh-yemu.{js,wasm} via emscripten, then stage the VM assets
# (kernel + opensbi + alpine-rootfs) so the browser can fetch them.
#
# Prereqs:
#   - Emscripten SDK on PATH (`source $HOME/.local/emsdk/emsdk_env.sh`).
#   - $REPO_ROOT/build-yemu-release/ populated by `../build-image.sh`
#     (gives us the kernel + opensbi + rootfs).
#
# Output ($REPO_ROOT/build-webasm-yemu-release/):
#   index.html              loader UI
#   serve.py                dev HTTP server
#   picomesh.cfg               tinyemu VM config (MEMFS paths)
#   picomesh-yemu.js           emscripten loader
#   picomesh-yemu.wasm         compiled tinyemu + slirp + temu
#   assets/
#     kernel-riscv64.bin
#     opensbi-fw_jump.elf
#     alpine-rootfs.img

set -Eeuo pipefail
trap 'rc=$?; echo "FAILED: rc=$rc line=$LINENO cmd: $BASH_COMMAND" >&2' ERR

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$HERE/../../../.." && pwd)"

# All build artifacts at repo root, never inside the source tree.
YEMU_BUILD="$REPO_ROOT/build-yemu-release"
OUT="$REPO_ROOT/build-webasm-yemu-release"

# Prefer the operator's emsdk install over any system emscripten —
# yetty's install-emscripten.sh drops one at ~/.local/emsdk that's known
# to match the upstream LLVM/wasm-ld bundling. Distro packages (notably
# the nix-packaged emscripten) have shipped with wasm-ld out of sync
# with emcc, producing `unknown argument: --no-stack-first` at link
# time — the emsdk install is the safe path.
EMSDK="${EMSDK:-$HOME/.local/emsdk}"
if [ -x "$EMSDK/upstream/emscripten/emcc" ]; then
    export PATH="$EMSDK/upstream/emscripten:$EMSDK:$PATH"
fi
if ! command -v emcmake >/dev/null 2>&1; then
    echo "FAIL: emcmake not on PATH — install emsdk and source emsdk_env.sh" >&2
    echo "  see $REPO_ROOT/README.md for the install one-liner" >&2
    exit 1
fi

for asset in kernel-riscv64.bin opensbi-fw_jump.elf alpine-rootfs.img; do
    if [ ! -f "$YEMU_BUILD/$asset" ]; then
        echo "FAIL: missing $YEMU_BUILD/$asset" >&2
        echo "  run: make -C $REPO_ROOT build-yemu-release" >&2
        exit 1
    fi
done

# tinyemu has no UART. The QEMU-targeted opensbi-fw_dynamic.bin emits
# via uart8250 → goes into the void under wasm. opensbi-fw_jump.elf
# from build-tools/3rdparty/opensbi/ uses htif/SBI-console which
# produces actual output through the bridge_console_write path. Both
# variants are emitted by the same opensbi recipe in build-yemu-release/
# under build-image.sh — we just pick the right one here.
LOCAL_OPENSBI="$YEMU_BUILD/opensbi-fw_jump.elf"

mkdir -p "$OUT"

echo "==> emcmake configure → $OUT"
emcmake cmake -B "$OUT" -S "$HERE" -DCMAKE_BUILD_TYPE=Release

echo "==> emmake build"
cmake --build "$OUT" --parallel

echo "==> staging VM assets under $OUT/assets/"
mkdir -p "$OUT/assets"
cp -vL "$YEMU_BUILD/kernel-riscv64.bin" "$OUT/assets/"
cp -vL "$LOCAL_OPENSBI"                 "$OUT/assets/opensbi-fw_jump.elf"
cp -vL "$YEMU_BUILD/alpine-rootfs.img"  "$OUT/assets/"

# Stage the webapp static tree (CSS + the Monaco / EasyMDE rich-text editor
# vendor) under $OUT/static. The in-browser pages render inside a `srcdoc`
# iframe whose base origin is THIS dev server (serve.py / GitHub Pages), so
# Monaco's runtime AMD require()/XHR for /static/vendor/monaco/vs/* resolves
# HERE — not through the slirp bridge (which only inlines the initial <script>
# tags, never Monaco's dynamically-loaded modules + workers). Without this the
# rich-text editor can't load and the editor area is blank.
STATIC_SRC="$REPO_ROOT/assets/picoforge/static"
if [ -d "$STATIC_SRC" ]; then
    echo "==> staging webapp static tree (Monaco/EasyMDE) under $OUT/static/"
    rm -rf "$OUT/static"
    cp -a "$STATIC_SRC" "$OUT/static"
else
    echo "WARN: $STATIC_SRC missing — the in-browser rich-text editor will not load" >&2
fi

echo
echo "Build ready under $OUT/:"
ls -lh "$OUT/picomesh-yemu.js" "$OUT/picomesh-yemu.wasm" "$OUT/assets/"
echo
echo "Serve locally:"
echo "  python3 $OUT/serve.py 8000 $OUT"
echo "  open http://127.0.0.1:8000/"
