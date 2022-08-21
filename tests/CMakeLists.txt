file(GLOB_RECURSE ALL_TEST_FILES "*.cpp")

foreach(TEST_FILE ${ALL_TEST_FILES})
    get_filename_component(TEST_FILE_NAME ${TEST_FILE} NAME)
    string(REPLACE ".cpp" "" TEST_TARGET ${TEST_FILE_NAME})
    message("  - ${TEST_FILE} --> ${TEST_TARGET}")

    add_executable(${TEST_TARGET}_test ${TEST_FILE})

    set_target_properties(${TEST_TARGET}_test
            PROPERTIES OUTPUT_NAME ${TEST_TARGET}
            )

    set_target_properties(${TEST_TARGET}_test PROPERTIES LINK_SEARCH_START_STATIC ON)
    set_target_properties(${TEST_TARGET}_test PROPERTIES LINK_SEARCH_END_STATIC ON)

    set_target_properties(${TEST_TARGET}_test PROPERTIES OUTPUT_NAME test_${TEST_TARGET})

    target_link_libraries(${TEST_TARGET}_test PRIVATE netLib_static -static-libgcc -static-libstdc++ -static)
endforeach(TEST_FILE ${ALL_TEST_FILES})
