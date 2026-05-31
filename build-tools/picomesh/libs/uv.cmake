# libuv — async I/O. Built locally via build-tools/3rdparty/libuv/_build.sh.
#
# Static target name: uv

include_guard(GLOBAL)
include(${PICOMESH_ROOT}/build-tools/picomesh/3rdparty-fetch.cmake)

if(TARGET uv)
    return()
endif()

picomesh_3rdparty_fetch(libuv _LIBUV_DIR)

set(_LIBUV_LIB "")
foreach(_CAND libuv_a.a libuv.a)
    if(EXISTS "${_LIBUV_DIR}/lib/${_CAND}")
        set(_LIBUV_LIB "${_LIBUV_DIR}/lib/${_CAND}")
        break()
    endif()
endforeach()
if(NOT _LIBUV_LIB)
    message(FATAL_ERROR "libuv: no static archive found under ${_LIBUV_DIR}/lib")
endif()
if(NOT EXISTS "${_LIBUV_DIR}/include/uv.h")
    message(FATAL_ERROR "libuv: include/uv.h missing in ${_LIBUV_DIR}")
endif()

add_library(uv STATIC IMPORTED GLOBAL)
set_target_properties(uv PROPERTIES
    IMPORTED_LOCATION             "${_LIBUV_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_LIBUV_DIR}/include"
)
target_link_libraries(uv INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m rt)

message(STATUS "libuv: prebuilt @${PICOMESH_3RDPARTY_libuv_VERSION} (${_LIBUV_LIB})")
