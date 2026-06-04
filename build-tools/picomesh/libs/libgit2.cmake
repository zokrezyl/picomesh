# libgit2 — local-filesystem git library. GPLv2 with a linking
# exception that explicitly permits combining with code under any
# other license (BSL, proprietary, MIT, …). See the libgit2 source
# tarball's COPYING file or _build.sh comments for the exact wording.

include_guard(GLOBAL)
include(${PICOMESH_ROOT}/build-tools/picomesh/3rdparty-fetch.cmake)

if(TARGET libgit2)
    return()
endif()

picomesh_3rdparty_fetch(libgit2 _LG_DIR)

set(_LG_LIB "${_LG_DIR}/lib/libgit2.a")
if(NOT EXISTS "${_LG_LIB}")
    message(FATAL_ERROR "libgit2: ${_LG_LIB} missing")
endif()
if(NOT EXISTS "${_LG_DIR}/include/git2.h")
    message(FATAL_ERROR "libgit2: include/git2.h missing")
endif()

add_library(libgit2 STATIC IMPORTED GLOBAL)
set_target_properties(libgit2 PROPERTIES
    IMPORTED_LOCATION             "${_LG_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_LG_DIR}/include"
)
# Minimal libgit2 (no HTTPS, no SSH) built against zlib-ng instead of the
# bundled zlib (USE_BUNDLED_ZLIB=OFF — see the _build.sh). libgit2.a therefore
# carries UNDEFINED deflate/inflate symbols; the zlib-ng IMPORTED lib resolves
# them at the final link (it must follow libgit2 on the link line, which it does
# as a libgit2 INTERFACE dependency). Beyond zlib-ng it only needs libc +
# pthread — plus librt on glibc for clock_gettime.
include(${PICOMESH_ROOT}/build-tools/picomesh/libs/zlib-ng.cmake)
find_package(Threads REQUIRED)
target_link_libraries(libgit2 INTERFACE zlib-ng Threads::Threads rt)

message(STATUS "libgit2: prebuilt v${PICOMESH_3RDPARTY_libgit2_VERSION} (${_LG_LIB})")
