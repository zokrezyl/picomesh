yaafc_add_plugin(storage
    SOURCES kv.c sql.c
    EXTRA_INCLUDES $<TARGET_PROPERTY:sqlite3,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(yaafc_plugin_storage PUBLIC sqlite3)
