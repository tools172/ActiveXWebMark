#include "theme.h"

namespace awm::ui {

namespace {

// Prefers the shell's message font so Chinese Windows gets Microsoft YaHei UI
// while English Windows gets Segoe UI.
const wchar_t* uiFaceName()
{
    static wchar_t face[LF_FACESIZE] = {};
    static bool initialized = false;
    if (!initialized) {
        NONCLIENTMETRICSW metrics = {};
        metrics.cbSize = sizeof(metrics);
        if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
            wcscpy_s(face, metrics.lfMessageFont.lfFaceName);
        }
        if (face[0] == L'\0') {
            wcscpy_s(face, L"Segoe UI");
        }
        initialized = true;
    }
    return face;
}

} // namespace

Theme::Theme() = default;

Theme::~Theme()
{
    destroyFonts();
}

void Theme::initialize(UINT dpi)
{
    dpi_ = dpi == 0 ? 96 : dpi;
    destroyFonts();
}

void Theme::setDpi(UINT dpi)
{
    const UINT normalized = dpi == 0 ? 96 : dpi;
    if (normalized == dpi_) {
        return;
    }
    dpi_ = normalized;
    destroyFonts();
}

int Theme::px(int logicalPx) const
{
    return ::MulDiv(logicalPx, static_cast<int>(dpi_), 96);
}

HFONT Theme::uiFont() const
{
    if (uiFont_ == nullptr) {
        uiFont_ = createFont(9, FW_NORMAL, uiFaceName());
    }
    return uiFont_;
}

HFONT Theme::uiFontSemibold() const
{
    if (uiFontSemibold_ == nullptr) {
        uiFontSemibold_ = createFont(9, FW_SEMIBOLD, uiFaceName());
    }
    return uiFontSemibold_;
}

HFONT Theme::smallFont() const
{
    if (smallFont_ == nullptr) {
        smallFont_ = createFont(8, FW_NORMAL, uiFaceName());
    }
    return smallFont_;
}

HFONT Theme::monoFont() const
{
    if (monoFont_ == nullptr) {
        monoFont_ = createFont(11, FW_NORMAL, L"Consolas");
    }
    return monoFont_;
}

HFONT Theme::titleFont() const
{
    if (titleFont_ == nullptr) {
        titleFont_ = createFont(22, FW_SEMIBOLD, uiFaceName());
    }
    return titleFont_;
}

void Theme::destroyFonts()
{
    auto destroy = [](HFONT& font) {
        if (font != nullptr) {
            ::DeleteObject(font);
            font = nullptr;
        }
    };
    destroy(uiFont_);
    destroy(uiFontSemibold_);
    destroy(smallFont_);
    destroy(monoFont_);
    destroy(titleFont_);
}

HFONT Theme::createFont(int pointSize, int weight, const wchar_t* face) const
{
    return ::CreateFontW(-::MulDiv(pointSize, static_cast<int>(dpi_), 72), 0, 0, 0, weight,
                         FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, face);
}

} // namespace awm::ui
