picomesh_add_plugin(sharded_storage
    SOURCES sharded_storage.c
    EXTRA_INCLUDES
        $<TARGET_PROPERTY:libmdbx,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(picomesh_plugin_sharded_storage PUBLIC libmdbx)
