file(GLOB_RECURSE TEST_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(run_tests ${TEST_SRC_FILES})
target_link_libraries(run_tests PUBLIC
    gtest_main
)
