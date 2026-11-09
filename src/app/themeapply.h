#pragma once

#include "theme/theme_catalog.h"
#include "ui/ncscrollbar.h"
#include "ui/theme.h"

namespace awm {

/// Converts a theme catalog entry into the GDI palette used by the custom
/// window chrome. Kept in the app layer so `awm_ui` stays independent of the
/// core theme catalog.
ui::Palette MakePalette(const theme::ThemeDefinition& theme);

/// True when the palette's main surface is dark. Used to choose the Windows
/// native control theme (scrollbars, tree/list, menus).
bool IsDarkPalette(const ui::Palette& palette);

/// Scrollbar colors derived from a theme (track = page, thumb = blend of page
/// and text, arrow = secondary text). Keeps native scrollbars in step with the
/// preview scrollbar colors.
ui::ScrollbarColors MakeScrollbarColors(const theme::ThemeDefinition& theme);
ui::ScrollbarColors MakeScrollbarColors(const ui::Palette& palette);

/// Scrollbar colors for the navigation tree: the track follows the sidebar
/// background instead of the document surface.
ui::ScrollbarColors MakeNavScrollbarColors(const ui::Palette& palette);

} // namespace awm
