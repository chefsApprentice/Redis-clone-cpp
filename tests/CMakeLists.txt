file(GLOB_RECURSE TEST_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(redis_tests
    ${TEST_SOURCES}
)

target_link_libraries(redis_tests
    PRIVATE
        utils
        project_warnings
        project_sanitizers
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(redis_tests)
