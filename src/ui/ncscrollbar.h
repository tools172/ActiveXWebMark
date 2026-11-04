#pragma once

#include <Windows.h>

namespace awm::ui {

/// Colors used to repaint the non-client scrollbars of a native control.
struct ScrollbarColors {
    COLORREF track = RGB(255, 255, 255);
    COLORREF thumb = RGB(192, 196, 201);
    COLORREF arrow = RGB(87, 96, 106);
};

/// Installs (once) or updates a subclass that repaints the vertical and
/// horizontal non-client scrollbars of `hwnd` with the given colors.
///
/// The native scrollbar geometry and hit-testing are left untouched: the
/// control's window procedure still draws and handles the scrollbars, and the
/// subclass only paints the theme colors over them. Safe to call repeatedly;
/// colors are stored per window and removed automatically on WM_NCDESTROY.
void SetThemedScrollbar(HWND hwnd, const ScrollbarColors& colors);

/// Removes the painter and its colors. Normally unnecessary because the
/// subclass cleans itself up on WM_NCDESTROY.
void RemoveThemedScrollbar(HWND hwnd);

} // namespace awm::ui
