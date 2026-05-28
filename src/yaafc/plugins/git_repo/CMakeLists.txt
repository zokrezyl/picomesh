yaafc_add_plugin(git_repo
    SOURCES store.c
    EXTRA_INCLUDES $<TARGET_PROPERTY:libgit2,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(yaafc_plugin_git_repo PUBLIC libgit2)

