# awm_editor — Markdown source editor backed by Scintilla.
add_library(awm_editor STATIC
    scintillaeditor.cpp
)

target_include_directories(awm_editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Scintilla (vcpkg) ships SciLexer.lib without a CMake config package, so link
# the full paths explicitly, selecting the debug/release variant per config.
target_link_libraries(awm_editor PUBLIC awm_core
    optimized "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/SciLexer.lib"
    debug "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/SciLexer.lib"
    imm32
)
