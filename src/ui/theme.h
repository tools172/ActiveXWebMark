#pragma once

#include <Windows.h>

namespace awm::ui {

/// Simple RGB triple used by the palette (independent of COLORREF byte order).
struct Color {
    BYTE r = 0;
    BYTE g = 0;
    BYTE b = 0;
};

inline COLORREF ToColorRef(const Color& color)
{
    return RGB(color.r, color.g, color.b);
}

/// Refined light palette shared by every custom-drawn control.
struct Palette {
    Color windowBg{245, 246, 248};      ///< Application/client background.
    Color surface{255, 255, 255};       ///< Preview, editor and tab surface.
    Color navBg{250, 251, 252};         ///< Navigation panel header.
    Color tabHover{237, 239, 242};      ///< Hovered inactive tab.
    Color divider{225, 228, 232};       ///< 1px separators and outer borders.
    Color textPrimary{31, 35, 40};      ///< Main text.
    Color textSecondary{110, 119, 129}; ///< Muted labels and inactive tabs.
    Color accent{47, 111, 235};         ///< Selection/active accents.
    Color accentSoft{232, 240, 254};    ///< Accent-tinted hover backgrounds.
    Color danger{214, 69, 69};          ///< Error text.
    Color dirtyDot{47, 111, 235};       ///< Unsaved-change indicator.
    Color gutterBg{247, 248, 250};      ///< Editor line-number margin.
    Color currentLine{246, 249, 255};   ///< Editor current-line highlight.
    Color selection{216, 232, 255};     ///< Editor/preview selection.
};

/// DPI-aware fonts and palette used by the custom controls.
///
/// Fonts are created lazily per DPI; changing the DPI discards them so the next
/// paint recreates them at the new scale. The object is not thread-safe and is
/// owned by MainWindow (controls keep a non-owning pointer).
class Theme {
public:
    Theme();
    ~Theme();

    Theme(const Theme&) = delete;
    Theme& operator=(const Theme&) = delete;

    void initialize(UINT dpi);
    void setDpi(UINT dpi);

    UINT dpi() const { return dpi_; }

    /// Scales a 96-DPI logical pixel value to the current DPI.
    int px(int logicalPx) const;

    const Palette& palette() const { return palette_; }

    HFONT uiFont() const;
    HFONT uiFontSemibold() const;
    HFONT smallFont() const;
    HFONT monoFont() const;
    HFONT titleFont() const;

private:
    void destroyFonts();
    HFONT createFont(int pointSize, int weight, const wchar_t* face) const;

    UINT dpi_ = 96;
    Palette palette_;
    mutable HFONT uiFont_ = nullptr;
    mutable HFONT uiFontSemibold_ = nullptr;
    mutable HFONT smallFont_ = nullptr;
    mutable HFONT monoFont_ = nullptr;
    mutable HFONT titleFont_ = nullptr;
};

} // namespace awm::ui
