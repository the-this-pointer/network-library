file(GLOB_RECURSE NetLib_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)


if(BUILD_SHARED_LIBS)
    add_library(netLib_shared SHARED ${NetLib_SRC_FILES})
    target_include_directories(netLib_shared
            PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/inc>
            $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>
            )


    if(WIN32)
        target_compile_definitions(netLib_shared
                PRIVATE
                WIN32_LEAN_AND_MEAN
                _WINSOCK_DEPRECATED_NO_WARNINGS
                )
        target_link_libraries(netLib_shared PUBLIC ws2_32)
    endif()
endif()

add_library(netLib_static STATIC ${NetLib_SRC_FILES})

target_include_directories(netLib_static
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>
        )

if(WIN32)
    target_compile_definitions(netLib_static
            PRIVATE
            WIN32_LEAN_AND_MEAN
            _WINSOCK_DEPRECATED_NO_WARNINGS
            )
    target_link_libraries(netLib_static PUBLIC ws2_32)
endif()
