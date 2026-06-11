# libco — stackful coroutines (ISC).
#
# Locally built via build-tools/3rdparty/libco/_build.sh, then exposed
# here as the IMPORTED static target `co`.

include_guard(GLOBAL)
include(${PICOMESH_ROOT}/build-tools/picomesh/3rdparty-fetch.cmake)

if(TARGET co)
    return()
endif()

picomesh_3rdparty_fetch(libco _LIBCO_DIR)

set(_LIBCO_LIB "")
foreach(_CAND libco.a libco.lib)  # .a on POSIX, .lib on MSVC
    if(EXISTS "${_LIBCO_DIR}/lib/${_CAND}")
        set(_LIBCO_LIB "${_LIBCO_DIR}/lib/${_CAND}")
        break()
    endif()
endforeach()
if(NOT _LIBCO_LIB)
    message(FATAL_ERROR "libco: no libco.a/libco.lib under ${_LIBCO_DIR}/lib")
endif()
if(NOT EXISTS "${_LIBCO_DIR}/include/libco.h")
    message(FATAL_ERROR "libco: include/libco.h missing after fetch")
endif()

add_library(co STATIC IMPORTED GLOBAL)
set_target_properties(co PROPERTIES
    IMPORTED_LOCATION             "${_LIBCO_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_LIBCO_DIR}/include"
)

message(STATUS "libco: prebuilt @${PICOMESH_3RDPARTY_libco_VERSION} (${_LIBCO_LIB})")
