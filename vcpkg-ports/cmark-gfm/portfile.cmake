# No dynamic link for MSVC: the app ships a self-contained executable.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+)\\.([0-9]+)$" "\\1.gfm.\\2" GFM_VERSION "${VERSION}")

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO github/cmark-gfm
    REF ${GFM_VERSION}
    SHA512 435298fcf782dfc5b64c578ac839759b9d5cd0c08eb90d6702f26278062a0f4887c65c18e89e2c9f6be23f10dd835c769a7e0f8c934be068b6754dcca30cdd7c
    HEAD_REF master
    PATCHES
        add-feature-tools.patch
)

set(CMARK_STATIC ON)
set(CMARK_SHARED OFF)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCMARK_TESTS=OFF
        -DCMARK_SHARED=${CMARK_SHARED}
        -DCMARK_STATIC=${CMARK_STATIC}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake)
vcpkg_cmake_config_fixup(PACKAGE_NAME cmark-gfm-extensions CONFIG_PATH lib/cmake-gfm-extensions)
# No vcpkg_fixup_pkgconfig(): MSVC consumers use the CMake targets, and the
# pkgconf acquisition step requires an MSYS download that is not needed here.

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
