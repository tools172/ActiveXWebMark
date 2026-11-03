# ActiveXWebMark unit tests (GoogleTest).
find_package(GTest CONFIG REQUIRED)

add_executable(awm_core_tests
    core/headingparser_test.cpp
    core/discountengine_test.cpp
    core/cmarkengine_test.cpp
    core/pmhhighlighter_test.cpp
    core/settings_test.cpp
    core/stylemerge_test.cpp
    core/pathutil_test.cpp
    core/fileutil_test.cpp
    core/document_session_test.cpp
    core/recent_files_test.cpp
    core/previewhtml_test.cpp
    core/update_test.cpp
)

target_link_libraries(awm_core_tests PRIVATE awm_core GTest::gtest_main)

add_test(NAME awm_core_tests COMMAND awm_core_tests)

# Pure layout/hierarchy tests (no Windows window creation).
add_executable(awm_layout_tests
    layout/geometry_test.cpp
    layout/navrows_test.cpp
    layout/scrollmap_test.cpp
)

target_link_libraries(awm_layout_tests PRIVATE awm_layout GTest::gtest_main)

add_test(NAME awm_layout_tests COMMAND awm_layout_tests)

# Scroll synchronization policy tests (fake editor/preview, no windows).
add_executable(awm_sync_tests
    sync/scrollsync_test.cpp
)

target_link_libraries(awm_sync_tests PRIVATE awm_sync GTest::gtest_main)

add_test(NAME awm_sync_tests COMMAND awm_sync_tests)
