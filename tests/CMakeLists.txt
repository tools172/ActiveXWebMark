# ActiveXWebMark unit tests (GoogleTest).
find_package(GTest CONFIG REQUIRED)

add_executable(awm_core_tests
    core/headingparser_test.cpp
    core/discountengine_test.cpp
    core/cmarkengine_test.cpp
    core/pmhhighlighter_test.cpp
    core/settings_test.cpp
    core/stylemerge_test.cpp
)

target_link_libraries(awm_core_tests PRIVATE awm_core GTest::gtest_main)

add_test(NAME awm_core_tests COMMAND awm_core_tests)
