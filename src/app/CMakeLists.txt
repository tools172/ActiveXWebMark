# awm_app — WTL executable.
add_executable(awm_app WIN32
    main.cpp
    mainwindow.cpp
)

# The product executable is named ActiveXWebMark.exe regardless of the CMake target name.
set_target_properties(awm_app PROPERTIES OUTPUT_NAME "ActiveXWebMark")

target_include_directories(awm_app PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/wtl/Include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(awm_app PRIVATE awm_core awm_editor awm_preview)
