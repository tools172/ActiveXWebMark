# awm_core — engine/UI-agnostic core logic (document model, markdown engine,
# syntax highlighting, settings). No GUI dependency; unit-testable headlessly.
add_library(awm_core STATIC
    version.cpp
    document/markdowndocument.cpp
    document/headingparser.cpp
    document/markdownloader.cpp
    document/documentformatregistry.cpp
    history/recent_files.cpp
    markdown/discountmarkdownengine.cpp
    markdown/cmarkengine.cpp
    markdown/previewhtml.cpp
    session/document_session.cpp
    highlight/pmhhighlighter.cpp
    highlight/stylemerge.cpp
    settings/isettings.cpp
    settings/jsonfilesettings.cpp
    update/updateinfo.cpp
    util/stringutil.cpp
    util/fileutil.cpp
    util/pathutil.cpp
    # PEG Markdown Highlight parser (vendored in third_party/; C sources).
    ${CMAKE_SOURCE_DIR}/third_party/peg-markdown-highlight/pmh_parser.c
)

# The release workflow passes the tag version as AWM_VERSION so a tagged build
# always reports the released version. Local builds fall back to the project
# version declared in the top-level CMakeLists.txt.
if(AWM_VERSION)
    set(AWM_VERSION_STRING "${AWM_VERSION}")
else()
    set(AWM_VERSION_STRING "${PROJECT_VERSION}")
endif()
target_compile_definitions(awm_core PRIVATE AWM_VERSION_STRING="${AWM_VERSION_STRING}")

target_include_directories(awm_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party/peg-markdown-highlight
)

# Discount + cmark (Markdown engines) and nlohmann-json (settings) from vcpkg.
find_package(discount CONFIG REQUIRED)
find_package(cmark CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Vendored third-party C sources (generated PEG parser) produce expected noise at
# /W4; compile them at a quieter level.
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/third_party/peg-markdown-highlight/pmh_parser.c
    PROPERTIES COMPILE_OPTIONS "/W3;/wd4996;/wd4267"
)

target_link_libraries(awm_core
    PUBLIC
        discount::libmarkdown
        cmark::cmark_static
        nlohmann_json::nlohmann_json
)
