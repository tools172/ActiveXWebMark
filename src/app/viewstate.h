#pragma once

#include "layout/geometry.h"

namespace awm {

/// Global view preferences shared by all document tabs.
///
/// The navigation side is fixed to the right; ratios are stored separately for
/// the two split orientations so switching orientation does not lose a layout.
struct ViewSettings {
    bool showEditor = false;
    bool showNav = true;
    ui::Orientation split = ui::Orientation::Horizontal;
    double sideBySideRatio = 0.55;
    double stackedRatio = 0.55;
    int navWidth = 280; ///< Logical (96-DPI) pixels.
    bool scrollSync = true;
};

} // namespace awm
