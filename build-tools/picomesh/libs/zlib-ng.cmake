# zlib-ng — SIMD-accelerated drop-in zlib (built in ZLIB_COMPAT mode, so it
# exposes the stock zlib API as lib/libz.a + include/zlib.h). libgit2 is built
# against it (USE_BUNDLED_ZLIB=OFF) and carries undefined deflate/inflate
# symbols; this IMPORTED lib resolves them at the final link. Prebuilt tarball
# is fetched (download-or-build) by 3rdparty-fetch.cmake.

include_guard(GLOBAL)
include(${PICOMESH_ROOT}/build-tools/picomesh/3rdparty-fetch.cmake)

if(TARGET zlib-ng)
    return()
endif()

picomesh_3rdparty_fetch(zlib-ng _ZNG_DIR)

set(_ZNG_LIB "")
foreach(_CAND libz.a zlibstatic.lib zlib.lib)  # .a on POSIX, .lib on MSVC
    if(EXISTS "${_ZNG_DIR}/lib/${_CAND}")
        set(_ZNG_LIB "${_ZNG_DIR}/lib/${_CAND}")
        break()
    endif()
endforeach()
if(NOT _ZNG_LIB)
    message(FATAL_ERROR "zlib-ng: no libz.a/zlibstatic.lib under ${_ZNG_DIR}/lib")
endif()
if(NOT EXISTS "${_ZNG_DIR}/include/zlib.h")
    message(FATAL_ERROR "zlib-ng: include/zlib.h missing")
endif()

add_library(zlib-ng STATIC IMPORTED GLOBAL)
set_target_properties(zlib-ng PROPERTIES
    IMPORTED_LOCATION             "${_ZNG_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_ZNG_DIR}/include"
)

message(STATUS "zlib-ng: prebuilt v${PICOMESH_3RDPARTY_zlib-ng_VERSION} (${_ZNG_LIB})")
