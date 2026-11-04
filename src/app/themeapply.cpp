#include "themeapply.h"

namespace awm {

namespace {

ui::Color ToUiColor(const theme::ThemeColor& color)
{
    ui::Color result;
    result.r = color.r;
    result.g = color.g;
    result.b = color.b;
    return result;
}

} // namespace

ui::Palette MakePalette(const theme::ThemeDefinition& theme)
{
    const theme::ChromeColors& colors = theme.chrome;
    ui::Palette palette;
    palette.windowBg = ToUiColor(colors.windowBg);
    palette.surface = ToUiColor(colors.surface);
    palette.navBg = ToUiColor(colors.navBg);
    palette.titleBarBg = ToUiColor(colors.titleBarBg);
    palette.titleBarInactive = ToUiColor(colors.titleBarInactive);
    palette.tabHover = ToUiColor(colors.tabHover);
    palette.buttonHover = ToUiColor(colors.buttonHover);
    palette.buttonPressed = ToUiColor(colors.buttonPressed);
    palette.divider = ToUiColor(colors.divider);
    palette.textPrimary = ToUiColor(colors.textPrimary);
    palette.textSecondary = ToUiColor(colors.textSecondary);
    palette.accent = ToUiColor(colors.accent);
    palette.accentSoft = ToUiColor(colors.accentSoft);
    palette.gutterBg = ToUiColor(colors.gutterBg);
    palette.currentLine = ToUiColor(colors.currentLine);
    palette.selection = ToUiColor(colors.selection);
    palette.danger = ToUiColor(colors.danger);
    palette.dirtyDot = ToUiColor(colors.dirtyDot);
    // closeHover / closePressed intentionally keep the Windows red defaults.
    return palette;
}

} // namespace awm
