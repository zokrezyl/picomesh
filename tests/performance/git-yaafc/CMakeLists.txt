# git-yaafc performance harness.
#
# Self-contained load generator: plain C + pthreads + POSIX sockets, no
# yaafc libraries linked in (it speaks the gateway's HTTP contract, so
# it also runs against the yaapp gateway). Built as `yaafc-perf`.
find_package(Threads REQUIRED)

add_executable(yaafc-perf yaafc-perf.c)
target_link_libraries(yaafc-perf PRIVATE Threads::Threads)
set_target_properties(yaafc-perf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
