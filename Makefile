# picomesh — yet another application framework in C
#
# Top-level wrapper around CMake/Ninja. The build name format is
# `build-<platform>-<config>` to match the yetty convention.
#
# Targets (run `make` with no args to list):
#   build-desktop-release    — release build (default platform = host)
#   build-desktop-debug      — debug build
#   compile_commands.json    — symlink to the last configured build database
#   clean                    — wipe every build-* directory
#   help                     — this list

CMAKE   ?= cmake
NINJA   ?= ninja
JOBS    ?= $(shell nproc 2>/dev/null || echo 4)

BUILD_DIR_RELEASE := build-desktop-release
BUILD_DIR_DEBUG   := build-desktop-debug
BUILD_DIR_RISCV   := build-linux-riscv64-release
BUILD_DIR_YEMU    := build-yemu-release
BUILD_DIR_DEPLOY  := build-deploy

.PHONY: help all build-desktop-release build-desktop-debug build-linux-riscv64-release build-deploy build-yemu-release build-webasm-yemu-release clean

help:
	@awk 'BEGIN{FS=":"} /^## / {sub(/^## /,""); print "  " $$0}' $(MAKEFILE_LIST)

## build-desktop-release  configure + build the release variant
build-desktop-release:
	$(CMAKE) -S . -B $(BUILD_DIR_RELEASE) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@ln -sfn $(BUILD_DIR_RELEASE)/compile_commands.json compile_commands.json
	$(CMAKE) --build $(BUILD_DIR_RELEASE) --parallel $(JOBS)

## build-desktop-debug    configure + build the debug variant
build-desktop-debug:
	$(CMAKE) -S . -B $(BUILD_DIR_DEBUG) -G Ninja -DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@ln -sfn $(BUILD_DIR_DEBUG)/compile_commands.json compile_commands.json
	$(CMAKE) --build $(BUILD_DIR_DEBUG) --parallel $(JOBS)

## build-deploy             stage build-deploy/ — host-runnable picomesh tree
##                          (binary + yaml + frontend + run.sh). Reused
##                          verbatim by build-yemu-release as the in-VM
##                          /opt/picoforge payload.
build-deploy: build-desktop-release
	bash scenarios/picoforge/deploy/stage.sh

## build-yemu-release       bake the riscv64 VM rootfs at
##                          build-yemu-release/ (kernel + opensbi +
##                          alpine-rootfs.img with /opt/picoforge/
##                          injected from build-deploy/). Needs sudo
##                          for losetup/mount.
##
## Prereqs:
##   - make build-linux-riscv64-release   (cross-compiled picomesh binary)
##   - make build-deploy                  (host deploy tree)
build-yemu-release: build-linux-riscv64-release build-deploy
	bash scenarios/picoforge/yemu/build-image.sh

## build-webasm-yemu-release  compile tinyemu to wasm (picomesh-yemu.{js,wasm})
##                            for the in-browser demo at
##                            build-webasm-yemu-release/. Stages kernel +
##                            opensbi + alpine-rootfs from build-yemu-release/
##                            under build-webasm-yemu-release/assets/.
##
## Prereqs:
##   - make build-yemu-release          (VM rootfs + kernel)
##   - Emscripten SDK installed at $$HOME/.local/emsdk (or EMSDK=…)
build-webasm-yemu-release:
	bash scenarios/picoforge/yemu/web/build.sh

## build-linux-riscv64-release  cross-compile picomesh + picoforge-webapp for
##                              riscv64 (static, for the yemu demo VM
##                              or any riscv64 Linux target). Needs
##                              gcc-riscv64-linux-gnu / g++-riscv64-linux-gnu
##                              on the host (Ubuntu: apt install
##                              gcc-riscv64-linux-gnu g++-riscv64-linux-gnu).
build-linux-riscv64-release:
	$(CMAKE) -S . -B $(BUILD_DIR_RISCV) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/build-tools/picomesh/cross/linux-riscv64.cmake
	$(CMAKE) --build $(BUILD_DIR_RISCV) --parallel $(JOBS)
	@file $(BUILD_DIR_RISCV)/picomesh 2>/dev/null || true

## clean                  wipe every build-*/ artifact directory
clean:
	rm -rf $(BUILD_DIR_RELEASE) $(BUILD_DIR_DEBUG) $(BUILD_DIR_RISCV) \
	       $(BUILD_DIR_YEMU) $(BUILD_DIR_DEPLOY) \
	       build-webasm-yemu-release compile_commands.json

