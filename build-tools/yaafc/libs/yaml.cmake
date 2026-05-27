# libyaml — YAML 1.1 parser/emitter (C).
#
# Built locally via build-tools/3rdparty/libyaml/_build.sh, then
# exposed here as the IMPORTED static target `yaml`.

include_guard(GLOBAL)
include(${YAAFC_ROOT}/build-tools/yaafc/3rdparty-fetch.cmake)

if(TARGET yaml)
    return()
endif()

yaafc_3rdparty_fetch(libyaml _LIBYAML_DIR)

set(_LIBYAML_LIB "")
foreach(_CAND libyaml.a yaml.lib)
    if(EXISTS "${_LIBYAML_DIR}/lib/${_CAND}")
        set(_LIBYAML_LIB "${_LIBYAML_DIR}/lib/${_CAND}")
        break()
    endif()
endforeach()
if(NOT _LIBYAML_LIB)
    message(FATAL_ERROR "libyaml: no static archive in ${_LIBYAML_DIR}/lib")
endif()
if(NOT EXISTS "${_LIBYAML_DIR}/include/yaml.h")
    message(FATAL_ERROR "libyaml: yaml.h missing in ${_LIBYAML_DIR}/include")
endif()

add_library(yaml STATIC IMPORTED GLOBAL)
set_target_properties(yaml PROPERTIES
    IMPORTED_LOCATION             "${_LIBYAML_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_LIBYAML_DIR}/include"
    INTERFACE_COMPILE_DEFINITIONS "YAML_DECLARE_STATIC"
)

message(STATUS "libyaml: prebuilt v${YAAFC_3RDPARTY_libyaml_VERSION} (${_LIBYAML_LIB})")
