#pragma once

#include <string>

#include "theme/theme_catalog.h"

namespace awm {

/// Builds the complete, IE11-safe preview stylesheet for a theme: document
/// typography, theme colors, Prism token colors and preview find highlights.
///
/// The result deliberately contains no CSS custom properties or `var()`:
/// IE11's MSHTML does not support them.
std::string BuildPreviewCss(const theme::ThemeDefinition& theme);

/// Opacity for the floating code-block copy icon. Returns the most translucent
/// value (no lower than 0.55) whose blended color still reaches 3:1 contrast
/// against the theme's code-block background, so no border is needed.
double CopyIconOpacity(const theme::ThemeDefinition& theme);

} // namespace awm
