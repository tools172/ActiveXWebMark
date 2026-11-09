# awm_app — WTL executable.
#
# The icon resource is generated next to the build tree so the .rc file can
# point at the icon with an absolute path (rc.exe resolves relative resource
# paths against its working directory, not against the .rc file).
set(AWM_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
set(AWM_PRISM_JS "${CMAKE_SOURCE_DIR}/third_party/prism/prism.bundle.min.js")
set(AWM_MERMAID_DIR "${CMAKE_SOURCE_DIR}/third_party/mermaid")
set(AWM_MERMAID_SIDECAR "${AWM_MERMAID_DIR}/sidecar.cjs")
set(AWM_MERMAID_BUNDLE "${AWM_MERMAID_DIR}/mermaid.min.js")
set(AWM_RESOURCE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/resource.h")

if(NOT EXISTS "${AWM_PRISM_JS}")
    message(FATAL_ERROR "Missing Prism bundle: ${AWM_PRISM_JS}")
endif()
if(NOT EXISTS "${AWM_MERMAID_SIDECAR}" OR NOT EXISTS "${AWM_MERMAID_BUNDLE}")
    message(FATAL_ERROR "Missing Mermaid sidecar files in ${AWM_MERMAID_DIR}")
endif()
# The bundle is embedded as RCDATA. Reconfigure and recompile the resource
# object when it changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AWM_PRISM_JS}")

configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)

# Recompile the resource object when the icon or the Prism bundle changes
# (generators that support OBJECT_DEPENDS; otherwise touch the generated .rc).
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    PROPERTIES OBJECT_DEPENDS "${AWM_APP_ICON};${AWM_PRISM_JS}")

add_executable(awm_app WIN32
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    main.cpp
    mainwindow.cpp
    documentview.cpp
    diagramservice.cpp
    apppaths.cpp
    appsettings.cpp
    browseremulation.cpp
    darkmode.cpp
    previewassets.cpp
    themeapply.cpp
    themedialog.cpp
    dialogs.cpp
    fileassociation.cpp
    filedialogs.cpp
    recentfilesdialog.cpp
    update/updateclient.cpp
    update/updatedialog.cpp
    update/updateservice.cpp
)

# The product executable is named ActiveXWebMark.exe regardless of the CMake target name.
set_target_properties(awm_app PROPERTIES OUTPUT_NAME "ActiveXWebMark")

target_include_directories(awm_app PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/wtl/Include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(awm_app PRIVATE
    awm_core
    awm_editor
    awm_preview
    awm_ui
    awm_sync
    advapi32
    comdlg32
    shell32
    shlwapi
    ole32
    oleaut32
    winhttp
    bcrypt
)

# The Node sidecar and Mermaid bundle are runtime files, not linked into the
# executable. Keep them beside the exe so the app can find <exe>/mermaid/.
set_property(TARGET awm_app APPEND PROPERTY LINK_DEPENDS
    "${AWM_MERMAID_SIDECAR};${AWM_MERMAID_BUNDLE}")
add_custom_command(TARGET awm_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:awm_app>/mermaid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${AWM_MERMAID_SIDECAR}"
        "${AWM_MERMAID_BUNDLE}"
        "$<TARGET_FILE_DIR:awm_app>/mermaid"
    COMMENT "Copying Mermaid renderer files")

# PlantUML native files are fetched separately (tools/plantuml-native/fetch.ps1).
# Copy them next to the exe when present; the app falls back to the remote
# server or source text otherwise. Packaging requires the fetched files.
set(AWM_PLANTUML_STAGING "${CMAKE_SOURCE_DIR}/build/plantuml-native/staging")
add_custom_command(TARGET awm_app POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DSRC=${AWM_PLANTUML_STAGING}"
        "-DDST=$<TARGET_FILE_DIR:awm_app>/plantuml"
        -P "${CMAKE_SOURCE_DIR}/cmake/CopyPlantUmlNative.cmake"
    COMMENT "Copying PlantUML native files (if present)")
