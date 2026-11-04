#pragma once

#include <string>
#include <vector>

#include <Windows.h>

#include "layout/geometry.h"
#include "layout/titlebar_layout.h"
#include "theme.h"

namespace awm::ui {

/// Actions the title bar can request from its owner.
enum class TitleBarAction {
    None,
    OpenMenu,       ///< Open the popup menu at TitleBarEvent::menuIndex.
    Minimize,
    ToggleMaximize,
    Close,
    ShowSystemMenu, ///< The application icon was clicked.
};

struct TitleBarEvent {
    TitleBarAction action = TitleBarAction::None;
    int menuIndex = -1;
};

/// Modern owner-drawn title bar renderer.
///
/// The control is intentionally not a child window: MainWindow draws it in its
/// own client area and forwards mouse messages. That keeps WM_NCHITTEST on the
/// frame window authoritative, so dragging, double-click maximize, Snap
/// Layouts (Windows 11) and resize borders keep working without forwarding NC
/// messages across a child window boundary.
class TitleBar {
public:
    TitleBar() = default;

    void SetTheme(const Theme* theme);
    void SetMenus(std::vector<MenuLabel> menus);
    void SetIcon(HICON icon) { icon_ = icon; }
    void SetTitle(std::wstring title);
    void SetActive(bool active);
    void SetMaximized(bool maximized);

    bool active() const { return active_; }
    bool maximized() const { return maximized_; }

    /// Shows mnemonic underlines (Alt / F10 menu mode).
    void SetMenuMode(bool enabled);
    bool menuMode() const { return menuMode_; }
    /// Highlights one top-level menu while navigating with the keyboard.
    void SetHighlightedMenu(int index);
    int highlightedMenu() const { return highlightedMenu_; }
    /// Marks the menu that currently has an open popup.
    void SetOpenMenu(int index);
    int openMenu() const { return openMenu_; }

    /// Title bar height for the current DPI.
    int height() const;

    /// Recomputes the layout for a client width and returns the height.
    int Layout(int width);
    const TitleBarLayout& layout() const { return layout_; }

    int menuCount() const { return static_cast<int>(menus_.size()); }
    const MenuLabel& menuAt(int index) const;
    /// Returns the first top-level menu with `mnemonic` (case-insensitive), or -1.
    int mnemonicMenu(wchar_t mnemonic) const;

    /// Mouse handling. All points are in title-bar-local (client) coordinates.
    bool OnMouseMove(POINT client);
    bool OnMouseLeave();
    bool OnLButtonDown(POINT client);
    TitleBarEvent OnLButtonUp(POINT client);
    void CancelPress();
    bool hasPressed() const { return pressedPart_ != TitleBarHit::None; }

    int hoveredMenu() const { return hoveredMenu_; }
    TitleBarHit hoveredPart() const { return hoveredPart_; }
    TitleBarHit pressedPart() const { return pressedPart_; }
    int pressedMenu() const { return pressedMenu_; }

    TitleBarHit hitTest(POINT client, int* menuIndex = nullptr) const;
    /// Client-space rectangle of a top-level menu label, or an empty rect.
    RECT menuRect(int index) const;

    /// Test/automation hook: sets the hover state directly. The main window's
    /// diagnostic message uses this so UI smoke tests can verify hover painting
    /// without moving the physical cursor (which TrackMouseEvent would race).
    void SetHoverForTest(TitleBarHit part, int menuIndex);

    /// Paints the title bar using the rectangles from the last Layout().
    void Paint(HDC dc) const;

private:
    TitleBarMetrics metrics() const;
    int px(int logicalPx) const;
    std::vector<int> measureMenuWidths() const;
    void resolveHit(POINT client, TitleBarHit& part, int& menu) const;

    void paintMenu(HDC dc, int index) const;
    void paintTitle(HDC dc) const;
    void paintButton(HDC dc, const RectI& rect, TitleBarHit part) const;
    void paintGlyph(HDC dc, const RectI& rect, TitleBarHit part, COLORREF color) const;
    void paintMenuText(HDC dc, const MenuLabel& menu, const RectI& rect) const;

    const Theme* theme_ = nullptr;
    std::vector<MenuLabel> menus_;
    std::wstring title_;
    HICON icon_ = nullptr;

    TitleBarLayout layout_;

    bool active_ = true;
    bool maximized_ = false;
    bool menuMode_ = false;
    int highlightedMenu_ = -1;
    int openMenu_ = -1;

    TitleBarHit hoveredPart_ = TitleBarHit::None;
    int hoveredMenu_ = -1;
    TitleBarHit pressedPart_ = TitleBarHit::None;
    int pressedMenu_ = -1;
};

} // namespace awm::ui
