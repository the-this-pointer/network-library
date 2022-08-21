cmake_minimum_required(VERSION 3.22)
project(NetLib)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(WITH_TESTS "build with tests" ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_subdirectory(src)

if(WITH_TESTS)
    add_subdirectory(tests)
endif()