cmake_minimum_required(VERSION 3.24)
project(awm_spike_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(awm_spike_host main.cpp)

if(MSVC)
    target_compile_options(awm_spike_host PRIVATE /W4 /utf-8)
    target_compile_definitions(awm_spike_host PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

target_link_libraries(awm_spike_host PRIVATE user32)
