# awm_ui — reusable custom-drawn WTL controls (tabs, splitters, navigation).
# Geometry lives in awm_layout so it stays headlessly testable.
add_library(awm_ui STATIC
    theme.cpp
    doublebuffer.cpp
    controls/tabstrip.cpp
    controls/splitter.cpp
    controls/navpanel.cpp
    controls/dragline.cpp
)

target_include_directories(awm_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/wtl/Include
)

target_link_libraries(awm_ui PUBLIC
    awm_layout
    gdiplus
    comctl32
    uxtheme
    user32
    gdi32
)
