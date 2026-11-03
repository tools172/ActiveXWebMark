# awm_app — WTL executable.
#
# The icon resource is generated next to the build tree so the .rc file can
# point at the icon with an absolute path (rc.exe resolves relative resource
# paths against its working directory, not against the .rc file).
set(AWM_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
set(AWM_RESOURCE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/resource.h")
configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)

# Recompile the resource object when the icon changes (generators that support
# OBJECT_DEPENDS; otherwise touch the generated .rc after replacing the icon).
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    PROPERTIES OBJECT_DEPENDS "${AWM_APP_ICON}")

add_executable(awm_app WIN32
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    main.cpp
    mainwindow.cpp
    documentview.cpp
    apppaths.cpp
    appsettings.cpp
    dialogs.cpp
    filedialogs.cpp
    recentfilesdialog.cpp
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
    comdlg32
    shell32
    shlwapi
    ole32
    oleaut32
)
