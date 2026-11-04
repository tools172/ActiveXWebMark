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

} // namespace awm
