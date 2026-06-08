# QA/Static analysis tools
# These tools require LLVM/Clang development libraries

# When configured directly (not via add_subdirectory) this becomes a top-level
# project. That is the intended way to build it on a machine whose default
# toolchain is a nix dev-profile gcc: configure with PATH=/usr/bin so the host
# compiler is picked, giving the binary the system glibc ELF interpreter so it
# can load the system libLLVM and its deps (libffi, …) via the normal ldconfig
# search — a nix-compiled binary cannot, its loader never looks in /lib.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    # C is needed too: find_package(LLVM) runs a C compile check (FindFFI).
    project(picomesh-result-checker C CXX)
endif()

if(DEFINED ENV{NIX_CC})
    # Nix: use LLVM/Clang from flake.nix env vars
    set(LLVM_DIR "$ENV{LLVM_DIR}")
    set(Clang_DIR "$ENV{Clang_DIR}")
else()
    # System: use LLVM 20
    if(NOT DEFINED LLVM_DIR)
        set(LLVM_DIR "/usr/lib/llvm-20/lib/cmake/llvm")
    endif()
    if(NOT DEFINED Clang_DIR)
        set(Clang_DIR "/usr/lib/cmake/clang-20")
    endif()
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)


message(STATUS "QA tools: Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "QA tools: Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "QA tools: Using ClangConfig.cmake in: ${Clang_DIR}")

# result-checker: ensures functions calling result-returning functions also return results
add_executable(result-checker
    result-checker.cpp
)

# See qa-tools/lib/naming-convention/CMakeLists.txt for why C++20.
set_target_properties(result-checker PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(result-checker PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_compile_definitions(result-checker PRIVATE
    ${LLVM_DEFINITIONS}
)

target_link_libraries(result-checker PRIVATE
    clangTooling
    clangBasic
    clangFrontend
    clangAST
    clangASTMatchers
)
