#pragma once

#include <functional>

#include "wtlinclude.h"
#include "theme.h"

namespace awm::ui {

/// Drag axis of a splitter window.
enum class SplitterAxis {
    Vertical,  ///< Vertical bar: drags left/right.
    Horizontal ///< Horizontal bar: drags up/down.
};

/// Thin custom splitter with a subtle line and hover/drag accent.
///
/// The control does not know about the panes it separates; it reports the
/// cursor position in parent client coordinates and the owner re-lays out.
class Splitter : public CWindowImpl<Splitter> {
public:
    DECLARE_WND_CLASS_EX(L"AwmSplitter", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    struct Callbacks {
        std::function<void(int)> onDrag;    ///< Position in parent client coords.
        std::function<void(int)> onDragEnd; ///< Final position when the drag ends.
    };

    bool Create(HWND parent, const RECT& rc, UINT id, SplitterAxis axis);
    void SetTheme(const Theme* theme);
    void SetAxis(SplitterAxis axis);
    void SetCallbacks(Callbacks callbacks);
    void OnDpiChanged();

    BEGIN_MSG_MAP(Splitter)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
        MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
    END_MSG_MAP()

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSetCursor(UINT, WPARAM, LPARAM, BOOL&);

    int cursorPosition() const;
    void notifyDrag();

    const Theme* theme_ = nullptr;
    SplitterAxis axis_ = SplitterAxis::Vertical;
    Callbacks callbacks_;
    bool dragging_ = false;
    bool hovered_ = false;
    bool trackingMouse_ = false;
};

} // namespace awm::ui
