#include "controls/splitter.h"

#include <algorithm>
#include <utility>

namespace awm::ui {

namespace {

void fillRectColor(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &rc, brush);
        ::DeleteObject(brush);
    }
}

} // namespace

bool Splitter::Create(HWND parent, const RECT& rc, UINT id, SplitterAxis axis)
{
    axis_ = axis;
    RECT rect = rc;
    return CWindowImpl<Splitter>::Create(parent, rect, nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, id) !=
           nullptr;
}

void Splitter::SetTheme(const Theme* theme)
{
    theme_ = theme;
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void Splitter::SetAxis(SplitterAxis axis)
{
    if (axis_ == axis) {
        return;
    }
    axis_ = axis;
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void Splitter::SetCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

void Splitter::OnDpiChanged()
{
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

LRESULT Splitter::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);

    const Palette palette = theme_ != nullptr ? theme_->palette() : Palette{};
    fillRectColor(dc, client, ToColorRef(palette.windowBg));

    // While dragging, the target is shown by the host's DragLine; keep this
    // control showing the current (static) divider instead of an accent.
    const bool active = hovered_ && !dragging_;
    const COLORREF color =
        ToColorRef(active ? palette.accent : palette.divider);

    if (axis_ == SplitterAxis::Vertical) {
        const int thickness = active ? 2 : 1;
        const int x = std::max(0, (static_cast<int>(client.right) - thickness) / 2);
        RECT line{x, client.top, x + thickness, client.bottom};
        fillRectColor(dc, line, color);
    } else {
        const int thickness = active ? 2 : 1;
        const int y = std::max(0, (static_cast<int>(client.bottom) - thickness) / 2);
        RECT line{client.left, y, client.right, y + thickness};
        fillRectColor(dc, line, color);
    }

    EndPaint(&ps);
    return 0;
}

LRESULT Splitter::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT Splitter::OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&)
{
    if (!dragging_) {
        dragging_ = true;
        hovered_ = true;
        ::SetCapture(m_hWnd);
        Invalidate(FALSE);
        notifyDrag();
    }
    return 0;
}

LRESULT Splitter::OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&)
{
    if (dragging_) {
        const int position = cursorPosition();
        dragging_ = false;
        if (::GetCapture() == m_hWnd) {
            ::ReleaseCapture();
        }
        if (callbacks_.onDragEnd) {
            callbacks_.onDragEnd(position);
        }
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT Splitter::OnMouseMove(UINT, WPARAM, LPARAM, BOOL&)
{
    if (dragging_) {
        notifyDrag();
        return 0;
    }

    if (!trackingMouse_) {
        TRACKMOUSEEVENT tracking = {};
        tracking.cbSize = sizeof(tracking);
        tracking.dwFlags = TME_LEAVE;
        tracking.hwndTrack = m_hWnd;
        ::TrackMouseEvent(&tracking);
        trackingMouse_ = true;
    }

    if (!hovered_) {
        hovered_ = true;
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT Splitter::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&)
{
    trackingMouse_ = false;
    if (!dragging_ && hovered_) {
        hovered_ = false;
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT Splitter::OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&)
{
    if (dragging_) {
        dragging_ = false;
        hovered_ = false;
        if (callbacks_.onDragCancel) {
            callbacks_.onDragCancel();
        }
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT Splitter::OnSetCursor(UINT, WPARAM, LPARAM lParam, BOOL& handled)
{
    if (LOWORD(lParam) != HTCLIENT) {
        handled = FALSE;
        return 0;
    }
    ::SetCursor(::LoadCursorW(nullptr, axis_ == SplitterAxis::Vertical ? IDC_SIZEWE
                                                                      : IDC_SIZENS));
    return TRUE;
}

int Splitter::cursorPosition() const
{
    POINT point = {};
    ::GetCursorPos(&point);
    HWND parent = ::GetParent(m_hWnd);
    if (parent != nullptr) {
        ::ScreenToClient(parent, &point);
    }
    return axis_ == SplitterAxis::Vertical ? point.x : point.y;
}

void Splitter::notifyDrag()
{
    if (callbacks_.onDrag) {
        callbacks_.onDrag(cursorPosition());
    }
}

} // namespace awm::ui
