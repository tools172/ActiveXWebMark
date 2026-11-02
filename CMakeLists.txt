cmake_minimum_required(VERSION 3.24)

project(ActiveXWebMark VERSION 0.2.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global compile settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AWM_BUILD_TESTS "Build ActiveXWebMark unit tests" ON)

if(MSVC)
    # Common to C and C++; C++-specific conformance flags are gated below so
    # vendored C sources (PMH) compile cleanly.
    add_compile_options(/W4 /utf-8)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
    add_compile_definitions(
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
endif()

# ---------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/layout)
add_subdirectory(src/ui)

add_subdirectory(src/editor)
add_subdirectory(src/preview)
add_subdirectory(src/app)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(AWM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
