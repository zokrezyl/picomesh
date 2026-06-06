picomesh_add_plugin(github_authn
    SOURCES store.c
    # libcurl headers must be on the codegen clang pass's include path too,
    # not just the compile.
    EXTRA_INCLUDES
        $<TARGET_PROPERTY:libcurl,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(picomesh_plugin_github_authn PUBLIC libcurl)
