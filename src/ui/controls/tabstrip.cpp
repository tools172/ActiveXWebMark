#include "controls/tabstrip.h"

#include <windowsx.h>

#include <algorithm>

#include <gdiplus.h>

#include "doublebuffer.h"

namespace awm::ui {

namespace {

Gdiplus::Color toGdiColor(const Color& color)
{
    return Gdiplus::Color(255, color.r, color.g, color.b);
}

RECT toRect(const RectI& rect)
{
    return RECT{rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
}

void fillRoundedTop(Gdiplus::Graphics& graphics, const RECT& rc, float radius,
                    const Color& color)
{
    const float left = static_cast<float>(rc.left);
    const float top = static_cast<float>(rc.top);
    const float right = static_cast<float>(rc.right);
    const float bottom = static_cast<float>(rc.bottom);
    const float diameter = radius * 2.0f;

    Gdiplus::GraphicsPath path;
    path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
    path.AddLine(left + radius, top, right - radius, top);
    path.AddArc(right - diameter, top, diameter, diameter, 270.0f, 90.0f);
    path.AddLine(right, top + radius, right, bottom);
    path.AddLine(right, bottom, left, bottom);
    path.CloseFigure();

    Gdiplus::SolidBrush brush(toGdiColor(color));
    graphics.FillPath(&brush, &path);
}

// Border without the bottom edge so the selected tab visually joins the
// document surface below the strip.
void drawSelectedBorder(Gdiplus::Graphics& graphics, const RECT& rc, float radius,
                        const Color& color)
{
    const float left = static_cast<float>(rc.left);
    const float top = static_cast<float>(rc.top);
    const float right = static_cast<float>(rc.right);
    const float bottom = static_cast<float>(rc.bottom);
    const float diameter = radius * 2.0f;

    Gdiplus::GraphicsPath path;
    path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
    path.AddLine(left + radius, top, right - radius, top);
    path.AddArc(right - diameter, top, diameter, diameter, 270.0f, 90.0f);
    path.AddLine(right, top + radius, right, bottom);
    path.StartFigure();
    path.AddLine(left, top + radius, left, bottom);

    Gdiplus::Pen pen(toGdiColor(color), 1.0f);
    graphics.DrawPath(&pen, &path);
}

void drawCloseIcon(Gdiplus::Graphics& graphics, const RECT& rc, bool hovered,
                   const Palette& palette)
{
    const float left = static_cast<float>(rc.left);
    const float top = static_cast<float>(rc.top);
    const float right = static_cast<float>(rc.right);
    const float bottom = static_cast<float>(rc.bottom);

    if (hovered) {
        Gdiplus::SolidBrush background(toGdiColor(palette.accentSoft));
        graphics.FillEllipse(&background, Gdiplus::RectF(left, top, right - left, bottom - top));
    }

    const float inset = (right - left) * 0.33f;
    Gdiplus::Pen pen(toGdiColor(hovered ? palette.textPrimary : palette.textSecondary), 1.4f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    graphics.DrawLine(&pen, left + inset, top + inset, right - inset, bottom - inset);
    graphics.DrawLine(&pen, right - inset, top + inset, left + inset, bottom - inset);
}

void drawDirtyDot(Gdiplus::Graphics& graphics, const RECT& rc, const Color& color)
{
    const float centerX = (static_cast<float>(rc.left) + static_cast<float>(rc.right)) * 0.5f;
    const float centerY = (static_cast<float>(rc.top) + static_cast<float>(rc.bottom)) * 0.5f;
    const float radius = 3.0f;
    Gdiplus::SolidBrush brush(toGdiColor(color));
    graphics.FillEllipse(&brush,
                         Gdiplus::RectF(centerX - radius, centerY - radius, radius * 2.0f,
                                        radius * 2.0f));
}

void fillSolid(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &rc, brush);
        ::DeleteObject(brush);
    }
}

} // namespace

bool TabStrip::Create(HWND parent, const RECT& rc, UINT id)
{
    RECT rect = rc;
    return CWindowImpl<TabStrip>::Create(parent, rect, nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, id) !=
           nullptr;
}

void TabStrip::SetTheme(const Theme* theme)
{
    theme_ = theme;
    relayout();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void TabStrip::SetItems(std::vector<Item> items, int activeIndex)
{
    items_ = std::move(items);
    activeIndex_ = (items_.empty() || activeIndex < 0)
                       ? -1
                       : std::clamp(activeIndex, 0, static_cast<int>(items_.size()) - 1);
    hoverIndex_ = -1;
    closeHoverIndex_ = -1;
    relayout();
    ensureActiveVisible();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void TabStrip::SetActiveIndex(int index)
{
    const int next = (items_.empty() || index < 0)
                         ? -1
                         : std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
    if (next == activeIndex_) {
        return;
    }
    activeIndex_ = next;
    ensureActiveVisible();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

int TabStrip::preferredHeight() const
{
    return theme_ != nullptr ? theme_->px(38) : 38;
}

void TabStrip::SetCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

void TabStrip::OnDpiChanged()
{
    relayout();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void TabStrip::relayout()
{
    if (!IsWindow() || theme_ == nullptr) {
        return;
    }

    HDC dc = GetDC();
    widths_ = measureWidths(dc);
    ReleaseDC(dc);

    firstVisible_ = clampTabScroll(firstVisible_, widths_);

    RECT client = {};
    GetClientRect(&client);
    const int top = theme_->px(7);
    const RectI area{0, top, static_cast<int>(client.right),
                     std::max(0, static_cast<int>(client.bottom) - top)};
    layout_ = layoutTabs(area, widths_, theme_->px(4), theme_->px(8), theme_->px(18),
                         firstVisible_);
}

void TabStrip::ensureActiveVisible()
{
    if (!IsWindow() || theme_ == nullptr) {
        return;
    }
    if (widths_.empty() || activeIndex_ < 0) {
        firstVisible_ = 0;
        relayout();
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    firstVisible_ = scrollToShow(activeIndex_, firstVisible_, client.right, widths_,
                                 theme_->px(4), theme_->px(8));
    relayout();
}

std::vector<int> TabStrip::measureWidths(HDC dc) const
{
    std::vector<int> widths;
    widths.reserve(items_.size());
    if (theme_ == nullptr || dc == nullptr) {
        widths.assign(items_.size(), 140);
        return widths;
    }

    // Measure with the semibold face used for the selected tab so the title
    // never truncates merely because it is drawn slightly wider.
    const HGDIOBJ oldFont = ::SelectObject(dc, theme_->uiFontSemibold());
    const int minWidth = theme_->px(110);
    const int maxWidth = theme_->px(280);
    const int padding = theme_->px(14);
    const int extra = theme_->px(18) + theme_->px(14);

    for (const Item& item : items_) {
        const wchar_t* title = item.title.empty() ? L"未命名" : item.title.c_str();
        const int titleLength =
            item.title.empty() ? 3 : static_cast<int>(item.title.size());
        SIZE size = {};
        if (!::GetTextExtentPoint32W(dc, title, titleLength, &size)) {
            size.cx = minWidth;
        }
        const int width = size.cx + padding * 2 + extra;
        widths.push_back(std::clamp(width, minWidth, maxWidth));
    }

    ::SelectObject(dc, oldFont);
    return widths;
}

int TabStrip::hitTab(POINT client) const
{
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const TabSlot& slot = layout_.slots[i];
        if (!slot.visible) {
            continue;
        }
        const RECT rect = toRect(slot.rect);
        if (::PtInRect(&rect, client) != FALSE) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int TabStrip::hitClose(POINT client) const
{
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const TabSlot& slot = layout_.slots[i];
        if (!slot.visible || slot.closeRect.empty()) {
            continue;
        }
        const RECT rect = toRect(slot.closeRect);
        if (::PtInRect(&rect, client) != FALSE) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

LRESULT TabStrip::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(&ps);
    RECT client = {};
    GetClientRect(&client);

    DoubleBuffer buffer(hdc, client.right, client.bottom);
    paint(buffer.valid() ? buffer.dc() : hdc, client);
    if (buffer.valid()) {
        buffer.present();
    }

    EndPaint(&ps);
    return 0;
}

void TabStrip::paint(HDC dc, const RECT& client)
{
    if (theme_ == nullptr) {
        fillSolid(dc, client, RGB(245, 246, 248));
        return;
    }

    const Palette& palette = theme_->palette();
    fillSolid(dc, client, ToColorRef(palette.windowBg));

    // Bottom divider: the selected tab will paint over its own segment.
    RECT divider{0, client.bottom - 1, client.right, client.bottom};
    fillSolid(dc, divider, ToColorRef(palette.divider));

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    const float radius = static_cast<float>(theme_->px(8));

    // Inactive tabs first, selected tab last so it covers the divider.
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const TabSlot& slot = layout_.slots[i];
        if (!slot.visible || static_cast<int>(i) == activeIndex_) {
            continue;
        }
        if (static_cast<int>(i) == hoverIndex_) {
            fillRoundedTop(graphics, toRect(slot.rect), radius, palette.tabHover);
        }
    }

    if (activeIndex_ >= 0 && static_cast<std::size_t>(activeIndex_) < layout_.slots.size()) {
        const TabSlot& slot = layout_.slots[static_cast<std::size_t>(activeIndex_)];
        if (slot.visible) {
            const RECT tabRect = toRect(slot.rect);
            fillRoundedTop(graphics, tabRect, radius, palette.surface);
            drawSelectedBorder(graphics, tabRect, radius, palette.divider);

            const float accentTop = static_cast<float>(tabRect.top) + 1.0f;
            Gdiplus::Pen accent(toGdiColor(palette.accent), 2.0f);
            accent.SetStartCap(Gdiplus::LineCapRound);
            accent.SetEndCap(Gdiplus::LineCapRound);
            graphics.DrawLine(&accent, static_cast<float>(tabRect.left) + radius, accentTop,
                              static_cast<float>(tabRect.right) - radius, accentTop);
        }
    }

    // Icons / dirty dots.
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const TabSlot& slot = layout_.slots[i];
        if (!slot.visible || slot.closeRect.empty()) {
            continue;
        }
        const Item& item = items_[i];
        const bool closeHovered = static_cast<int>(i) == closeHoverIndex_;
        const bool showClose = closeHovered || static_cast<int>(i) == activeIndex_ ||
                               static_cast<int>(i) == hoverIndex_;
        if (closeHovered) {
            drawCloseIcon(graphics, toRect(slot.closeRect), true, palette);
        } else if (item.dirty) {
            drawDirtyDot(graphics, toRect(slot.closeRect), palette.dirtyDot);
        } else if (showClose) {
            drawCloseIcon(graphics, toRect(slot.closeRect), false, palette);
        }
    }

    graphics.Flush();

    // Text is rendered with GDI for crisp ClearType output.
    const int saved = ::SaveDC(dc);
    ::SetBkMode(dc, TRANSPARENT);
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const TabSlot& slot = layout_.slots[i];
        if (!slot.visible) {
            continue;
        }
        const Item& item = items_[i];
        const bool selected = static_cast<int>(i) == activeIndex_;
        const bool hovered = static_cast<int>(i) == hoverIndex_;
        ::SelectObject(dc, selected ? theme_->uiFontSemibold() : theme_->uiFont());
        ::SetTextColor(dc, ToColorRef(selected || hovered ? palette.textPrimary
                                                          : palette.textSecondary));

        RECT textRect = toRect(slot.rect);
        textRect.left += theme_->px(14);
        textRect.right = slot.closeRect.empty()
                             ? static_cast<LONG>(slot.rect.x + slot.rect.w - theme_->px(10))
                             : static_cast<LONG>(slot.closeRect.x - theme_->px(4));
        if (textRect.right <= textRect.left) {
            continue;
        }
        const wchar_t* title = item.title.empty() ? L"未命名" : item.title.c_str();
        ::DrawTextW(dc, title, -1, &textRect,
                    DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    ::RestoreDC(dc, saved);
}

LRESULT TabStrip::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT TabStrip::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    relayout();
    ensureActiveVisible();
    Invalidate(FALSE);
    return 0;
}

LRESULT TabStrip::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const int closeIndex = hitClose(pt);
    if (closeIndex >= 0) {
        if (callbacks_.onClose) {
            callbacks_.onClose(closeIndex);
        }
        return 0;
    }

    const int tabIndex = hitTab(pt);
    if (tabIndex >= 0 && callbacks_.onActivate) {
        callbacks_.onActivate(tabIndex);
    }
    return 0;
}

LRESULT TabStrip::OnLButtonDblClk(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (hitTab(pt) < 0 && callbacks_.onEmptyDoubleClick) {
        callbacks_.onEmptyDoubleClick();
    }
    return 0;
}

LRESULT TabStrip::OnMButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const int tabIndex = hitTab(pt);
    if (tabIndex >= 0 && callbacks_.onClose) {
        callbacks_.onClose(tabIndex);
    }
    return 0;
}

LRESULT TabStrip::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    if (!trackingMouse_) {
        TRACKMOUSEEVENT tracking = {};
        tracking.cbSize = sizeof(tracking);
        tracking.dwFlags = TME_LEAVE;
        tracking.hwndTrack = m_hWnd;
        ::TrackMouseEvent(&tracking);
        trackingMouse_ = true;
    }

    const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const int nextHover = hitTab(pt);
    const int nextClose = hitClose(pt);
    if (nextHover != hoverIndex_ || nextClose != closeHoverIndex_) {
        hoverIndex_ = nextHover;
        closeHoverIndex_ = nextClose;
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT TabStrip::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&)
{
    trackingMouse_ = false;
    if (hoverIndex_ != -1 || closeHoverIndex_ != -1) {
        hoverIndex_ = -1;
        closeHoverIndex_ = -1;
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT TabStrip::OnMouseWheel(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (widths_.empty()) {
        return 0;
    }

    wheelDelta_ += GET_WHEEL_DELTA_WPARAM(wParam);
    int next = firstVisible_;
    while (wheelDelta_ >= WHEEL_DELTA) {
        --next;
        wheelDelta_ -= WHEEL_DELTA;
    }
    while (wheelDelta_ <= -WHEEL_DELTA) {
        ++next;
        wheelDelta_ += WHEEL_DELTA;
    }

    next = clampTabScroll(next, widths_);
    if (next != firstVisible_) {
        firstVisible_ = next;
        relayout();
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT TabStrip::OnSetCursor(UINT, WPARAM, LPARAM lParam, BOOL& handled)
{
    if (LOWORD(lParam) != HTCLIENT) {
        handled = FALSE;
        return 0;
    }

    POINT pt = {};
    ::GetCursorPos(&pt);
    ::ScreenToClient(m_hWnd, &pt);
    ::SetCursor(::LoadCursorW(nullptr, hitClose(pt) >= 0 ? IDC_HAND : IDC_ARROW));
    return TRUE;
}

} // namespace awm::ui
