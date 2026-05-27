# SQLite — embedded SQL engine. Local build via the 3rdparty pattern.

include_guard(GLOBAL)
include(${YAAFC_ROOT}/build-tools/yaafc/3rdparty-fetch.cmake)

if(TARGET sqlite3)
    return()
endif()

yaafc_3rdparty_fetch(libsqlite3 _SJ_DIR)

set(_SJ_LIB "${_SJ_DIR}/lib/libsqlite3.a")
if(NOT EXISTS "${_SJ_LIB}")
    message(FATAL_ERROR "sqlite3: ${_SJ_LIB} missing")
endif()
if(NOT EXISTS "${_SJ_DIR}/include/sqlite3.h")
    message(FATAL_ERROR "sqlite3: include/sqlite3.h missing")
endif()

add_library(sqlite3 STATIC IMPORTED GLOBAL)
set_target_properties(sqlite3 PROPERTIES
    IMPORTED_LOCATION             "${_SJ_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_SJ_DIR}/include"
)
# SQLite needs libdl for run-time loaded VFS shims; not used here
# (OMIT_LOAD_EXTENSION) but keep the linker happy on minimal images.
target_link_libraries(sqlite3 INTERFACE ${CMAKE_DL_LIBS} m)

message(STATUS "sqlite3: prebuilt v${YAAFC_3RDPARTY_libsqlite3_VERSION} (${_SJ_LIB})")
