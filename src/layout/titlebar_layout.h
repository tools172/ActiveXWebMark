#pragma once

#include <string>
#include <vector>

#include "geometry.h"

namespace awm::ui {

/// DPI-independent metrics for the custom title bar.
struct TitleBarMetrics {
    int height = 36;        ///< Total title bar height.
    int iconSize = 16;      ///< Application icon edge length.
    int iconMargin = 10;    ///< Left margin of the icon and gap around it.
    int menuGap = 2;        ///< Gap between two top-level menu labels.
    int menuPaddingX = 10;  ///< Horizontal padding inside a menu label.
    int buttonWidth = 46;   ///< Width of each window button (min/max/close).
};

/// Client-space rectangles produced by layoutTitleBar(). Coordinates are
/// relative to the title bar's top-left corner (normally the window client's
/// top-left corner).
struct TitleBarLayout {
    RectI bar;
    RectI icon;
    std::vector<RectI> menus;
    RectI title;
    RectI minimizeButton;
    RectI maximizeButton;
    RectI closeButton;
};

/// Which part of the title bar a point belongs to.
enum class TitleBarHit {
    None,      ///< Outside the title bar.
    Caption,   ///< Empty drag area (maps to HTCAPTION).
    Icon,      ///< Application icon (opens the system menu when clicked).
    Menu,      ///< A top-level menu label; see the menuIndex output.
    Minimize,  ///< Minimize button.
    Maximize,  ///< Maximize/restore button.
    Close,     ///< Close button.
};

/// Computes the title bar rectangles.
///
/// `menuWidths` contains the full clickable width of each top-level menu label
/// (text width plus horizontal padding), in left-to-right order. Widths are
/// treated as non-negative. When the bar is too narrow the title area collapses
/// to zero width; window buttons keep priority over menu labels in hit testing.
TitleBarLayout layoutTitleBar(const RectI& bar, const TitleBarMetrics& metrics,
                              const std::vector<int>& menuWidths);

/// Hit tests a point in title-bar-local coordinates.
///
/// Button areas take precedence over menu labels, menu labels over the icon,
/// and everything inside the bar otherwise maps to Caption. When the result is
/// TitleBarHit::Menu, `menuIndex` receives the label index if it is non-null.
TitleBarHit hitTestTitleBar(const TitleBarLayout& layout, int x, int y,
                            int* menuIndex = nullptr);

/// A top-level menu label parsed from a Windows menu string.
struct MenuLabel {
    std::wstring text;     ///< Display text without "&" / "(&X)".
    wchar_t mnemonic = 0;  ///< 0 when the label has no mnemonic.
};

/// Parses strings such as "文件(&F)" into text "文件" and mnemonic L'F'.
///
/// Rules:
///  * A trailing "(&X)" group is treated as the mnemonic and removed from the
///    display text (the common Win32 convention for Chinese labels).
///  * Otherwise every "&" is removed and the following character becomes the
///    mnemonic; "&&" produces a literal '&' with no mnemonic.
///  * A trailing TAB (accelerator) is dropped.
///  * Leading/trailing whitespace is trimmed from the display text.
MenuLabel parseMenuLabel(const std::wstring& raw);

} // namespace awm::ui
