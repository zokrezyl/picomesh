picomesh_add_plugin(git_repo
    SOURCES store.c
    # libgit2 headers, plus uthash (the per-shard repo-handle cache uses it; the
    # codegen's clang pass needs it on its include path, not just the compile).
    EXTRA_INCLUDES
        $<TARGET_PROPERTY:libgit2,INTERFACE_INCLUDE_DIRECTORIES>
        ${PICOMESH_ROOT}/include/uthash
)
target_link_libraries(picomesh_plugin_git_repo PUBLIC libgit2)

