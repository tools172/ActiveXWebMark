#pragma once

#include <Windows.h>

#include <string>

namespace awm {

/// Binary assets embedded in the executable and injected into the preview.
struct PreviewAssets {
    /// PrismJS bundle. Empty when the resource could not be loaded.
    std::string prismScript;

    bool available() const { return !prismScript.empty(); }
};

/// Loads the preview assets from `instance` (the executable module when null).
/// Never throws; missing resources simply produce empty assets.
PreviewAssets LoadPreviewAssets(HINSTANCE instance);

} // namespace awm
