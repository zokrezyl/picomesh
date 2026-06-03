picomesh_add_plugin(relational_storage
    SOURCES relational_storage.c
    EXTRA_INCLUDES
        $<TARGET_PROPERTY:sqlite3,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(picomesh_plugin_relational_storage PUBLIC sqlite3)
