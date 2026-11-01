# awm_preview — swappable HTML preview engine.
add_library(awm_preview STATIC
    previewfactory.cpp
    ie/iepreviewview.cpp
)

target_include_directories(awm_preview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(awm_preview PUBLIC awm_core)
