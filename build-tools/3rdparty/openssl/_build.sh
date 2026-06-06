#!/bin/bash
# Builds upstream OpenSSL ${VERSION} (openssl/openssl) for $TARGET_PLATFORM as
# a STATIC library using OpenSSL's native ./Configure + make build. This is the
# TLS backend libcurl links against (see ../libcurl/_build.sh), so the picomesh
# binary carries no runtime dependency on the host's libssl/libcrypto.
#
# Required env:
#   TARGET_PLATFORM   linux-x86_64 | linux-aarch64 | linux-riscv64 |
#                     macos-arm64 | macos-x86_64
#   OUTPUT_DIR        where the tarball is written
#
# Version is read from this directory's `version` file — single source of
# truth for both the upstream tag fetch (`openssl-<VER>`) and tarball naming.
#
# Tarball layout (consumed by build-tools/picomesh/libs/openssl.cmake):
#   lib/libssl.a
#   lib/libcrypto.a
#   include/openssl/...

set -Eeuo pipefail
trap 'rc=$?; echo "FAILED: rc=$rc line=$LINENO cmd: $BASH_COMMAND" >&2' ERR

: "${TARGET_PLATFORM:?TARGET_PLATFORM is required}"
: "${OUTPUT_DIR:?OUTPUT_DIR is required}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$SCRIPT_DIR/version")"
[ -n "$VERSION" ] || { echo "version file is empty" >&2; exit 1; }

# Upstream tag is `openssl-<VER>`; the GitHub archive top-level dir is
# `openssl-openssl-<VER>` (repo name + tag).
URL="https://github.com/openssl/openssl/archive/refs/tags/openssl-${VERSION}.tar.gz"
TARBALL_CACHE="${CACHE_DIR:-$HOME/.cache/picomesh-3rdparty}/openssl-${VERSION}.tar.gz"
WORK_DIR="${WORK_DIR:-/tmp/picomesh-3rdparty-openssl-$TARGET_PLATFORM}"
SRC_DIR="$WORK_DIR/openssl-${VERSION}"
INSTALL_DIR="$WORK_DIR/install-${TARGET_PLATFORM}"
STAGE="$WORK_DIR/stage-${TARGET_PLATFORM}"
TARBALL="$OUTPUT_DIR/openssl-${TARGET_PLATFORM}-${VERSION}.tar.gz"
NCPU="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

mkdir -p "$WORK_DIR" "$OUTPUT_DIR" "$(dirname "$TARBALL_CACHE")"

if [ ! -f "$TARBALL_CACHE" ]; then
    echo "==> downloading openssl/openssl ${VERSION}"
    curl -fL --retry 8 --retry-delay 5 --retry-all-errors \
        -o "$TARBALL_CACHE.part" "$URL"
    mv "$TARBALL_CACHE.part" "$TARBALL_CACHE"
fi
# openssl's native build is in-source (Configure writes Makefile + headers next
# to the sources), so extract a fresh copy per target.
if [ ! -d "$SRC_DIR" ]; then
    echo "==> extracting -> $SRC_DIR"
    rm -rf "$WORK_DIR/.extract"; mkdir -p "$WORK_DIR/.extract"
    tar -C "$WORK_DIR/.extract" -xzf "$TARBALL_CACHE"
    mv "$WORK_DIR/.extract/openssl-openssl-${VERSION}" "$SRC_DIR"
    rm -rf "$WORK_DIR/.extract"
fi
rm -rf "$INSTALL_DIR" "$STAGE"
mkdir -p "$INSTALL_DIR" "$STAGE"

# Static-only, no apps/docs/tests. --libdir=lib forces lib/ (not lib64/) so the
# stage layout is uniform across distros. SM2/SM3/SM4 (Chinese national crypto)
# are never on the TLS path to standard CAs and their x86_64 asm uses SM4-NI
# instructions some assemblers reject — disable them.
CFG_ARGS=(
    --prefix="$INSTALL_DIR"
    --libdir=lib
    no-shared
    no-tests
    no-apps
    no-docs
    no-makedepend
    no-sm2
    no-sm3
    no-sm4
)

case "$TARGET_PLATFORM" in
linux-x86_64)
    CFG_TARGET="linux-x86_64"
    ;;
linux-aarch64)
    : "${CROSS_PREFIX:=aarch64-linux-gnu-}"
    CFG_TARGET="linux-aarch64"
    export CC="${CROSS_PREFIX}gcc" AR="${CROSS_PREFIX}ar" RANLIB="${CROSS_PREFIX}ranlib"
    ;;
linux-riscv64)
    # openssl's Configure target is `linux64-riscv64` (Configurations/10-main.conf).
    : "${CROSS_PREFIX:=riscv64-linux-gnu-}"
    CFG_TARGET="linux64-riscv64"
    export CC="${CROSS_PREFIX}gcc" AR="${CROSS_PREFIX}ar" RANLIB="${CROSS_PREFIX}ranlib"
    ;;
macos-x86_64)
    CFG_TARGET="darwin64-x86_64-cc"
    ;;
macos-arm64)
    CFG_TARGET="darwin64-arm64-cc"
    ;;
*)
    echo "unknown TARGET_PLATFORM: $TARGET_PLATFORM" >&2
    exit 1
    ;;
esac

cd "$SRC_DIR"
echo "==> configuring openssl ${VERSION} for ${CFG_TARGET}"
perl ./Configure "$CFG_TARGET" "${CFG_ARGS[@]}"

echo "==> building (-j${NCPU})"
make -j"$NCPU" build_libs

echo "==> installing libs + headers"
make install_dev

# Modern openssl produces libssl.a + libcrypto.a directly; normalise from lib64/
# if the install picked that path.
mkdir -p "$STAGE/lib"
for _D in lib lib64; do
    if [ -d "$INSTALL_DIR/$_D" ]; then
        cp -a "$INSTALL_DIR/$_D/." "$STAGE/lib/"
    fi
done
cp -a "$INSTALL_DIR/include" "$STAGE/"

for _LIB in libssl.a libcrypto.a; do
    [ -f "$STAGE/lib/$_LIB" ] || { echo "missing library: $STAGE/lib/$_LIB" >&2; exit 1; }
done
[ -f "$STAGE/include/openssl/ssl.h" ] || { echo "missing headers: $STAGE/include/openssl/" >&2; exit 1; }

# The consumer wraps the archives as IMPORTED targets; the cmake/pkgconfig
# modules are unused.
rm -rf "$STAGE/lib/cmake" "$STAGE/lib/pkgconfig" "$STAGE/lib/engines"* "$STAGE/lib/ossl-modules"

echo "==> packaging -> $TARBALL"
tar -C "$STAGE" -czf "$TARBALL" .
echo "openssl $VERSION ($TARGET_PLATFORM) ready"
ls -lh "$TARBALL"
