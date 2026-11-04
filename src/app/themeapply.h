#pragma once

#include "theme/theme_catalog.h"
#include "ui/theme.h"

namespace awm {

/// Converts a theme catalog entry into the GDI palette used by the custom
/// window chrome. Kept in the app layer so `awm_ui` stays independent of the
/// core theme catalog.
ui::Palette MakePalette(const theme::ThemeDefinition& theme);

} // namespace awm
