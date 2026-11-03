#pragma once

#include <functional>
#include <string>
#include <vector>

#include "wtlinclude.h"
#include "theme.h"
#include "layout/geometry.h"

namespace awm::ui {

/// Modern, owner-drawn document tab strip.
///
/// Draws rounded tabs with a subtle accent on the selected item, a hover state,
/// a dirty dot, and a close button that appears on hover. Supports mouse-wheel
/// horizontal scrolling when the tabs overflow.
class TabStrip : public CWindowImpl<TabStrip> {
public:
    DECLARE_WND_CLASS_EX(L"AwmTabStrip", CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, COLOR_WINDOW)

    struct Item {
        std::wstring title;
        std::wstring tooltip; ///< Full path (unused by painting; reserved for tooltips).
        bool dirty = false;
    };

    struct Callbacks {
        std::function<void(int)> onActivate;
        std::function<void(int)> onClose;
        std::function<void()> onEmptyDoubleClick;
        std::function<void()> onToggleNav;
    };

    bool Create(HWND parent, const RECT& rc, UINT id);
    void SetTheme(const Theme* theme);
    void SetItems(std::vector<Item> items, int activeIndex);
    void SetActiveIndex(int index);
    int activeIndex() const { return activeIndex_; }
    int preferredHeight() const;
    void SetCallbacks(Callbacks callbacks);
    /// Reflects the nav sidebar state on the right-hand toggle button.
    void SetNavVisible(bool visible);
    void OnDpiChanged();

    /// Diagnostic hook used by tests/manual/tabstrip-smoke.ps1.
    /// wParam 0: bit0 overflowLeft, bit1 overflowRight, bit2 navVisible.
    /// wParam 1: firstVisible index. wParam 2: maxFirstVisible index.
    static constexpr UINT kTestStateMessage = WM_APP + 0x51;

    BEGIN_MSG_MAP(TabStrip)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMButtonDown)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
        MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
        MESSAGE_HANDLER(kTestStateMessage, OnTestState)
    END_MSG_MAP()

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseWheel(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSetCursor(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTestState(UINT, WPARAM, LPARAM, BOOL&);

    void relayout();
    void ensureActiveVisible();
    std::vector<int> measureWidths(HDC dc) const;
    int hitTab(POINT client) const;
    int hitClose(POINT client) const;
    bool hitNavButton(POINT client) const;
    bool hitLeftArrow(POINT client) const;
    bool hitRightArrow(POINT client) const;
    int navButtonSize() const;
    RECT navButtonRect() const;
    int tabAreaRight() const;
    int arrowSize() const;
    RECT leftArrowRect() const;
    RECT rightArrowRect() const;
    void scrollTabsBy(int delta);
    void updateTooltips();
    void paint(HDC dc, const RECT& client);

    const Theme* theme_ = nullptr;
    std::vector<Item> items_;
    std::vector<int> widths_;
    TabStripLayout layout_;
    Callbacks callbacks_;

    int activeIndex_ = -1;
    int hoverIndex_ = -1;
    int closeHoverIndex_ = -1;
    int firstVisible_ = 0;
    int maxFirstVisible_ = 0;
    int wheelDelta_ = 0;
    bool trackingMouse_ = false;
    bool navVisible_ = true;
    bool navHover_ = false;
    bool overflowLeft_ = false;
    bool overflowRight_ = false;
    bool leftArrowHover_ = false;
    bool rightArrowHover_ = false;
    bool tooltipAdded_ = false;
    CToolTipCtrl tooltip_;
};

} // namespace awm::ui
