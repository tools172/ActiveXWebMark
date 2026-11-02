# awm_layout — pure, GUI-independent geometry and TOC hierarchy helpers.
# No Windows headers, so it can be unit-tested headlessly.
add_library(awm_layout STATIC
    geometry.cpp
    navrows.cpp
)

target_include_directories(awm_layout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
