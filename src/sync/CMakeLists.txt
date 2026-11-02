# awm_sync — scroll synchronization policy between IEditorView and IPreviewView.
# Depends only on the abstract interfaces, so it can be tested with fakes.
add_library(awm_sync STATIC
    scrollsync.cpp
)

target_include_directories(awm_sync PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/editor
    ${CMAKE_SOURCE_DIR}/src/preview
)

target_link_libraries(awm_sync PUBLIC awm_core)
