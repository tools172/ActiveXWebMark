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
    core/diagramblocks_test.cpp
    core/theme_catalog_test.cpp
    core/theme_custom_test.cpp
    core/update_test.cpp
)

target_include_directories(awm_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/miniz)
target_link_libraries(awm_core_tests PRIVATE awm_core GTest::gtest_main)

add_test(NAME awm_core_tests COMMAND awm_core_tests)

# Pure layout/hierarchy tests (no Windows window creation).
add_executable(awm_layout_tests
    layout/geometry_test.cpp
    layout/navrows_test.cpp
    layout/scrollmap_test.cpp
    layout/titlebar_layout_test.cpp
)

target_link_libraries(awm_layout_tests PRIVATE awm_layout GTest::gtest_main)

add_test(NAME awm_layout_tests COMMAND awm_layout_tests)

# GDI-level tests for owner-drawn controls (no window creation required).
add_executable(awm_ui_tests
    ui/titlebar_test.cpp
    ui/theme_test.cpp
    ui/filedroptarget_test.cpp
)

target_link_libraries(awm_ui_tests PRIVATE awm_ui GTest::gtest_main)

add_test(NAME awm_ui_tests COMMAND awm_ui_tests)

# App-level theme/settings helpers. The app executable is not linkable as a
# library, so the small pure translation units are compiled into the test.
add_executable(awm_app_tests
    app/themeapply_test.cpp
    app/appsettings_test.cpp
    ${CMAKE_SOURCE_DIR}/src/app/themeapply.cpp
    ${CMAKE_SOURCE_DIR}/src/app/appsettings.cpp
)

target_include_directories(awm_app_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)

target_link_libraries(awm_app_tests PRIVATE
    awm_core
    awm_layout
    awm_ui
    user32
    GTest::gtest_main
)

add_test(NAME awm_app_tests COMMAND awm_app_tests)

# Scroll synchronization policy tests (fake editor/preview, no windows).
add_executable(awm_sync_tests
    sync/scrollsync_test.cpp
)

target_link_libraries(awm_sync_tests PRIVATE awm_sync GTest::gtest_main)

add_test(NAME awm_sync_tests COMMAND awm_sync_tests)
