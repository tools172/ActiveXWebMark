#include "controls/dragline.h"

#include <algorithm>

namespace awm::ui {

DragLine::~DragLine()
{
    // The popup is owned by the top-level window rather than by the host
    // view, so it must be torn down explicitly when a document view closes.
    if (IsWindow()) {
        DestroyWindow();
    }
}

bool DragLine::Create(HWND host)
{
    host_ = host;
    HWND owner = host != nullptr ? ::GetAncestor(host, GA_ROOT) : nullptr;
    RECT zero = {0, 0, 0, 0};
    return CWindowImpl<DragLine>::Create(owner, zero, nullptr, WS_POPUP,
                                         WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0U) != nullptr;
}

void DragLine::SetTheme(const Theme* theme)
{
    theme_ = theme;
    Invalidate(FALSE);
}

void DragLine::ShowVertical(int x, int top, int bottom)
{
    showClientRect(x, top, thickness(), std::max(0, bottom - top));
}

void DragLine::ShowHorizontal(int y, int left, int right)
{
    showClientRect(left, y, std::max(0, right - left), thickness());
}

void DragLine::Hide()
{
    if (IsWindow()) {
        ShowWindow(SW_HIDE);
    }
}

void DragLine::OnDpiChanged()
{
    Invalidate(FALSE);
}

void DragLine::showClientRect(int x, int y, int width, int height)
{
    if (!IsWindow() || host_ == nullptr) {
        return;
    }

    POINT origin{x, y};
    ::ClientToScreen(host_, &origin);
    ::SetWindowPos(m_hWnd, HWND_TOP, origin.x, origin.y, width, height,
                   SWP_SHOWWINDOW | SWP_NOACTIVATE);
    Invalidate(FALSE);
}

int DragLine::thickness() const
{
    return theme_ != nullptr ? std::max(1, theme_->px(2)) : 2;
}

LRESULT DragLine::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);
    const COLORREF color =
        theme_ != nullptr ? ToColorRef(theme_->palette().accent) : RGB(47, 111, 235);

    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &client, brush);
        ::DeleteObject(brush);
    }

    EndPaint(&ps);
    return 0;
}

LRESULT DragLine::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT DragLine::OnNcHitTest(UINT, WPARAM, LPARAM, BOOL&)
{
    // The splitter owns the mouse capture while dragging; never intercept.
    return HTTRANSPARENT;
}

LRESULT DragLine::OnMouseActivate(UINT, WPARAM, LPARAM, BOOL&)
{
    return MA_NOACTIVATE;
}

} // namespace awm::ui
