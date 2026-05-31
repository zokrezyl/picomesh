picomesh_add_plugin(storage
    SOURCES kv.c storage.c
    EXTRA_INCLUDES
        $<TARGET_PROPERTY:sqlite3,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:libmdbx,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(picomesh_plugin_storage PUBLIC sqlite3 libmdbx)
