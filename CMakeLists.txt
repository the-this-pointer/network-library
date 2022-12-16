cmake_minimum_required(VERSION 3.22)
project(NetLib)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(WITH_ASIO "enable asio server / client source codes" ON)

option(WITH_TESTS "build with tests" ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

if (WITH_ASIO)
    add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS ASIO_STANDALONE WITH_ASIO)
endif()

add_subdirectory(src)

if(WITH_TESTS)
    add_subdirectory(tests)
endif()