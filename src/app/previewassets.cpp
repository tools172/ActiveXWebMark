#include "previewassets.h"

#include <cstddef>

#include "resource.h"

namespace awm {

PreviewAssets LoadPreviewAssets(HINSTANCE instance)
{
    PreviewAssets assets;
    if (instance == nullptr) {
        instance = ::GetModuleHandleW(nullptr);
    }
    if (instance == nullptr) {
        return assets;
    }

    const HRSRC resource =
        ::FindResourceW(instance, MAKEINTRESOURCEW(IDR_PRISM_JS), RT_RCDATA);
    if (resource == nullptr) {
        return assets;
    }

    const DWORD size = ::SizeofResource(instance, resource);
    if (size == 0) {
        return assets;
    }

    const HGLOBAL handle = ::LoadResource(instance, resource);
    if (handle == nullptr) {
        return assets;
    }

    const auto* bytes = static_cast<const char*>(::LockResource(handle));
    if (bytes == nullptr) {
        return assets;
    }

    std::size_t length = size;
    // Tolerate a UTF-8 BOM and a single trailing NUL if the asset was produced
    // by an editor that adds one.
    if (length >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes += 3;
        length -= 3;
    }
    if (length > 0 && bytes[length - 1] == '\0') {
        --length;
    }

    assets.prismScript.assign(bytes, length);
    return assets;
}

} // namespace awm
