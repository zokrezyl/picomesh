# picoforge performance harness.
#
# Self-contained load generator: plain C + pthreads + POSIX sockets, no
# picomesh libraries linked in (it speaks the gateway's HTTP contract, so
# it also runs against the yaapp gateway). Built as `picoforge-perf`.
find_package(Threads REQUIRED)

add_executable(picoforge-perf picoforge-perf.c)
target_link_libraries(picoforge-perf PRIVATE Threads::Threads)
set_target_properties(picoforge-perf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
