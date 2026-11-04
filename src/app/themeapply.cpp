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

bool IsDarkPalette(const ui::Palette& palette)
{
    const double luminance = 0.2126 * palette.surface.r + 0.7152 * palette.surface.g +
                             0.0722 * palette.surface.b;
    return luminance < 128.0;
}

namespace {

COLORREF ToColorRef(const theme::ThemeColor& color)
{
    return RGB(color.r, color.g, color.b);
}

COLORREF ThumbOver(COLORREF background, COLORREF text)
{
    const auto mix = [](BYTE back, BYTE fore) {
        return static_cast<BYTE>((static_cast<int>(back) * 72 +
                                  static_cast<int>(fore) * 28 + 50) /
                                 100);
    };
    return RGB(mix(GetRValue(background), GetRValue(text)),
               mix(GetGValue(background), GetGValue(text)),
               mix(GetBValue(background), GetBValue(text)));
}

} // namespace

ui::ScrollbarColors MakeScrollbarColors(const theme::ThemeDefinition& theme)
{
    const theme::PreviewColors& preview = theme.preview;
    ui::ScrollbarColors colors;
    colors.track = ToColorRef(preview.pageBg);
    colors.thumb = ToColorRef(
        theme::Blend(preview.pageBg, preview.pageText, 0.28));
    colors.arrow = ToColorRef(theme.chrome.textSecondary);
    return colors;
}

ui::ScrollbarColors MakeScrollbarColors(const ui::Palette& palette)
{
    ui::ScrollbarColors colors;
    colors.track = ui::ToColorRef(palette.surface);
    colors.thumb = ThumbOver(colors.track, ui::ToColorRef(palette.textPrimary));
    colors.arrow = ui::ToColorRef(palette.textSecondary);
    return colors;
}

} // namespace awm
