#include "controls/tabstrip.h"

#include <windowsx.h>

#include <algorithm>

#include <gdiplus.h>

#include "doublebuffer.h"

namespace awm::ui {

namespace {

// Tool IDs 1..3 are the fixed controls (nav toggle and scroll arrows); tab
// tooltips start safely above them.
constexpr UINT kTabTooltipBase = 0x100;

Gdiplus::Color toGdiColor(const Color& color)
{
    return Gdiplus::Color(255, color.r, color.g, color.b);
}

RECT toRect(const RectI& rect)
{
    return RECT{rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
}

RectI toRectI(const RECT& rect)
{
    return RectI{rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
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

    const float inset = (right - left) * 0.30f;
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

void fillRoundedRect(Gdiplus::Graphics& graphics, const RECT& rc, float radius,
                     const Color& color)
{
    const float left = static_cast<float>(rc.left);
    const float top = static_cast<float>(rc.top);
    const float right = static_cast<float>(rc.right);
    const float bottom = static_cast<float>(rc.bottom);
    const float diameter = radius * 2.0f;

    Gdiplus::GraphicsPath path;
    path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(right - diameter, top, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(right - diameter, bottom - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(left, bottom - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();

    Gdiplus::SolidBrush brush(toGdiColor(color));
    graphics.FillPath(&brush, &path);
}

// Compact sidebar glyph: the right-hand column is filled while the sidebar is
// visible. Deliberately no chevron, so it cannot be mistaken for the tab
// overflow arrows that sit next to it.
void drawNavToggleIcon(Gdiplus::Graphics& graphics, const RECT& rc, bool visible, bool hovered,
                       const Palette& palette, int unit)
{
    const Color color = hovered ? palette.textPrimary
                                : (visible ? palette.accent : palette.textSecondary);
    const float centerX = (static_cast<float>(rc.left) + static_cast<float>(rc.right)) * 0.5f;
    const float centerY = (static_cast<float>(rc.top) + static_cast<float>(rc.bottom)) * 0.5f;
    const float scale = static_cast<float>(std::max(1, unit));
    const float panelWidth = scale * 3.4f;
    const float panelHeight = scale * 2.9f;
    const float panelLeft = centerX - panelWidth * 0.5f;
    const float panelTop = centerY - panelHeight * 0.5f;
    const float radius = scale * 0.65f;
    const float penWidth = std::max(1.0f, scale * 0.33f);
    const float dividerX = panelLeft + panelWidth * 0.64f;

    const Gdiplus::RectF panel(panelLeft, panelTop, panelWidth, panelHeight);
    const float diameter = radius * 2.0f;
    Gdiplus::GraphicsPath path;
    path.AddArc(panel.X, panel.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(panel.X + panel.Width - diameter, panel.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(panel.X + panel.Width - diameter, panel.Y + panel.Height - diameter, diameter,
                diameter, 0.0f, 90.0f);
    path.AddArc(panel.X, panel.Y + panel.Height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();

    if (visible) {
        Gdiplus::SolidBrush fill(
            Gdiplus::Color(90, palette.accent.r, palette.accent.g, palette.accent.b));
        graphics.FillRectangle(&fill,
                               Gdiplus::RectF(dividerX + penWidth, panel.Y + penWidth,
                                              panel.X + panel.Width - dividerX - penWidth * 2.0f,
                                              panel.Height - penWidth * 2.0f));
    }

    Gdiplus::Pen pen(toGdiColor(color), penWidth);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    graphics.DrawPath(&pen, &path);
    graphics.DrawLine(&pen, dividerX, panel.Y + penWidth * 0.5f, dividerX,
                      panel.Y + panel.Height - penWidth * 0.5f);
}

void drawScrollChevron(Gdiplus::Graphics& graphics, const RECT& rc, bool pointLeft, bool hovered,
                       const Palette& palette, int unit)
{
    if (hovered) {
        RECT background = rc;
        ::InflateRect(&background, -unit / 2, -unit);
        fillRoundedRect(graphics, background, static_cast<float>(std::max(1, unit)),
                        palette.tabHover);
    }

    const float centerX = (static_cast<float>(rc.left) + static_cast<float>(rc.right)) * 0.5f;
    const float centerY = (static_cast<float>(rc.top) + static_cast<float>(rc.bottom)) * 0.5f;
    const float halfWidth = static_cast<float>(std::max(1, unit)) * 0.55f;
    const float halfHeight = static_cast<float>(std::max(1, unit)) * 0.95f;
    const float direction = pointLeft ? -1.0f : 1.0f;

    Gdiplus::Pen pen(toGdiColor(hovered ? palette.textPrimary : palette.textSecondary),
                     std::max(1.0f, static_cast<float>(unit) * 0.4f));
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    graphics.DrawLine(&pen, centerX - halfWidth * direction, centerY - halfHeight,
                      centerX + halfWidth * direction, centerY);
    graphics.DrawLine(&pen, centerX + halfWidth * direction, centerY,
                      centerX - halfWidth * direction, centerY + halfHeight);
}

// Fades the tab strip edge into the strip background. The opaque end sits on the
// overflow side so tabs appear to slide underneath the scroll arrow.
void drawEdgeFade(Gdiplus::Graphics& graphics, const RECT& rc, bool opaqueAtLeft,
                  const Color& base)
{
    const Gdiplus::PointF start(static_cast<float>(rc.left), static_cast<float>(rc.top));
    const Gdiplus::PointF end(static_cast<float>(rc.right), static_cast<float>(rc.top));
    const Gdiplus::Color opaque(255, base.r, base.g, base.b);
    const Gdiplus::Color transparent(0, base.r, base.g, base.b);
    Gdiplus::LinearGradientBrush brush(start, end, opaqueAtLeft ? opaque : transparent,
                                       opaqueAtLeft ? transparent : opaque);
    graphics.FillRectangle(
        &brush, Gdiplus::RectF(static_cast<float>(rc.left), static_cast<float>(rc.top),
                               static_cast<float>(rc.right - rc.left),
                               static_cast<float>(rc.bottom - rc.top)));
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
    if (CWindowImpl<TabStrip>::Create(parent, rect, nullptr,
                                      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, id) == nullptr) {
        return false;
    }
    tooltip_.Create(m_hWnd);
    updateTooltips();
    return true;
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
    return theme_ != nullptr ? theme_->px(32) : 32;
}

void TabStrip::SetCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

void TabStrip::SetNavVisible(bool visible)
{
    if (navVisible_ == visible) {
        return;
    }
    navVisible_ = visible;
    updateTooltips();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void TabStrip::OnDpiChanged()
{
    relayout();
    ensureActiveVisible();
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
    const int top = theme_->px(5);
    const int height = std::max(0, static_cast<int>(client.bottom) - top);
    const int areaRight = tabAreaRight();
    const int padding = theme_->px(8);
    const int gap = theme_->px(3);
    const int arrow = arrowSize();

    const bool fits = widths_.empty() || tabsTotalWidth(widths_, gap) + padding * 2 <= areaRight;
    if (fits) {
        // Everything fits: scrolling is meaningless and must stay at zero.
        firstVisible_ = 0;
        maxFirstVisible_ = 0;
        overflowLeft_ = false;
        overflowRight_ = false;
        const RectI area{0, top, areaRight, height};
        layout_ = layoutTabs(area, widths_, gap, padding, theme_->px(16), firstVisible_);
        updateTooltips();
        return;
    }

    // Overflow: reserve both arrow slots so tab positions stay stable at the
    // ends, and never scroll past the point where the tail tab is flush.
    const int available = std::max(0, areaRight - arrow * 2);
    maxFirstVisible_ = tabStripTailStart(widths_, available, gap, padding);
    firstVisible_ = std::clamp(firstVisible_, 0, maxFirstVisible_);
    const RectI area{arrow, top, available, height};
    layout_ = layoutTabs(area, widths_, gap, padding, theme_->px(16), firstVisible_);
    overflowLeft_ = firstVisible_ > 0;
    overflowRight_ = firstVisible_ < maxFirstVisible_;
    updateTooltips();
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

    const int areaRight = tabAreaRight();
    const int padding = theme_->px(8);
    const int gap = theme_->px(3);
    const int arrow = arrowSize();
    const bool fits = tabsTotalWidth(widths_, gap) + padding * 2 <= areaRight;
    if (fits) {
        firstVisible_ = 0;
        relayout();
        return;
    }

    const int available = std::max(0, areaRight - arrow * 2);
    const int maxStart = tabStripTailStart(widths_, available, gap, padding);
    firstVisible_ =
        scrollToShow(activeIndex_, firstVisible_, available, widths_, gap, padding);
    firstVisible_ = std::clamp(firstVisible_, 0, maxStart);
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

    // Active and inactive tabs use the same font: the active state changes
    // only the surface, accent and text color, so activating a tab never
    // shifts or reflows its title. Measure with that same font.
    const HGDIOBJ oldFont = ::SelectObject(dc, theme_->smallFont());
    const int minWidth = theme_->px(88);
    const int maxWidth = theme_->px(220);
    const int padding = theme_->px(10);
    // The measured title already includes the left/right text padding; only a
    // small slot beside the close button is needed. Reserving a full 18+14px
    // here left a visible gap between the title and the close icon.
    const int extra = theme_->px(16);

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

int TabStrip::navButtonSize() const
{
    return theme_ != nullptr ? theme_->px(24) : 24;
}

RECT TabStrip::navButtonRect() const
{
    RECT client = {};
    GetClientRect(&client);
    const RectI rect = tabStripNavButtonRect(
        toRectI(client), navButtonSize(), theme_ != nullptr ? theme_->px(8) : 8);
    return toRect(rect);
}

int TabStrip::tabAreaRight() const
{
    const RECT button = navButtonRect();
    const int gap = theme_ != nullptr ? theme_->px(8) : 8;
    return tabStripAreaRight(static_cast<int>(button.left), gap);
}

bool TabStrip::hitNavButton(POINT client) const
{
    return rectContains(toRectI(navButtonRect()), client.x, client.y);
}

int TabStrip::arrowSize() const
{
    return theme_ != nullptr ? theme_->px(22) : 22;
}

RECT TabStrip::leftArrowRect() const
{
    RECT client = {};
    GetClientRect(&client);
    const int top = theme_ != nullptr ? theme_->px(5) : 5;
    const int bottom = std::max(top, static_cast<int>(client.bottom) - 1);
    return toRect(tabStripLeftArrowRect(top, bottom, arrowSize()));
}

RECT TabStrip::rightArrowRect() const
{
    const int right = tabAreaRight();
    const int top = theme_ != nullptr ? theme_->px(5) : 5;
    RECT client = {};
    GetClientRect(&client);
    const int bottom = std::max(top, static_cast<int>(client.bottom) - 1);
    return toRect(tabStripRightArrowRect(right, top, bottom, arrowSize()));
}

bool TabStrip::hitLeftArrow(POINT client) const
{
    if (!overflowLeft_) {
        return false;
    }
    return rectContains(toRectI(leftArrowRect()), client.x, client.y);
}

bool TabStrip::hitRightArrow(POINT client) const
{
    if (!overflowRight_) {
        return false;
    }
    return rectContains(toRectI(rightArrowRect()), client.x, client.y);
}

void TabStrip::scrollTabsBy(int delta)
{
    if (maxFirstVisible_ <= 0) {
        return;
    }
    const int next = tabStripScrollTarget(firstVisible_, delta, maxFirstVisible_);
    if (next == firstVisible_) {
        return;
    }
    firstVisible_ = next;
    relayout();
    Invalidate(FALSE);
}

void TabStrip::updateTooltips()
{
    if (!IsWindow() || !tooltip_.IsWindow()) {
        return;
    }

    const RECT navRect = navButtonRect();
    const wchar_t* navText = navVisible_ ? L"隐藏目录 (Ctrl+B)" : L"显示目录 (Ctrl+B)";
    const RECT leftRect = overflowLeft_ ? leftArrowRect() : RECT{0, 0, 0, 0};
    const RECT rightRect = overflowRight_ ? rightArrowRect() : RECT{0, 0, 0, 0};
    const wchar_t* leftText = L"显示前面的标签";
    const wchar_t* rightText = L"显示后面的标签";

    const auto addTool = [this](UINT id, const RECT& rect, const wchar_t* text) {
        TOOLINFO info = {};
        info.cbSize = sizeof(info);
        info.uFlags = TTF_SUBCLASS;
        info.hwnd = m_hWnd;
        info.uId = id;
        info.rect = rect;
        info.lpszText = const_cast<LPWSTR>(text);
        tooltip_.AddTool(&info);
    };

    if (!tooltipAdded_) {
        addTool(1, navRect, navText);
        addTool(2, leftRect, leftText);
        addTool(3, rightRect, rightText);
        tooltipAdded_ = true;
    } else {
        tooltip_.SetToolRect(m_hWnd, 1, &navRect);
        tooltip_.UpdateTipText(navText, m_hWnd, 1);
        tooltip_.SetToolRect(m_hWnd, 2, &leftRect);
        tooltip_.UpdateTipText(leftText, m_hWnd, 2);
        tooltip_.SetToolRect(m_hWnd, 3, &rightRect);
        tooltip_.UpdateTipText(rightText, m_hWnd, 3);
    }

    // One tool per tab: hovering shows the full path, which the ellipsized tab
    // title cannot. Hidden tabs keep an empty rect so they never trigger.
    tabTooltipTexts_.resize(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Item& item = items_[i];
        tabTooltipTexts_[i] = item.tooltip.empty()
                                  ? (item.title.empty() ? std::wstring(L"未命名") : item.title)
                                  : item.tooltip;
    }

    const int count = static_cast<int>(items_.size());
    for (int i = 0; i < count; ++i) {
        const UINT id = kTabTooltipBase + static_cast<UINT>(i);
        const auto index = static_cast<std::size_t>(i);
        const bool visible =
            index < layout_.slots.size() && layout_.slots[index].visible;
        const RECT rect = visible ? toRect(layout_.slots[index].rect) : RECT{0, 0, 0, 0};
        const wchar_t* text = tabTooltipTexts_[index].c_str();
        if (i < tabTooltipCount_) {
            tooltip_.SetToolRect(m_hWnd, id, &rect);
            tooltip_.UpdateTipText(text, m_hWnd, id);
        } else {
            addTool(id, rect, text);
        }
    }
    for (int i = count; i < tabTooltipCount_; ++i) {
        tooltip_.DelTool(m_hWnd, kTabTooltipBase + static_cast<UINT>(i));
    }
    tabTooltipCount_ = count;
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

    // Chrome-style separators between inactive tabs. They need enough contrast
    // to read at normal size, so use a semi-transparent text color instead of
    // the very light border color, and keep them tall enough to be noticed.
    // Separators touching the selected or hovered tab stay hidden.
    if (layout_.slots.size() > 1) {
        const float inset = static_cast<float>(theme_->px(5));
        const Gdiplus::Color separatorColor(140, palette.textSecondary.r,
                                            palette.textSecondary.g,
                                            palette.textSecondary.b);
        const float separatorWidth =
            static_cast<float>(std::max(1, theme_->px(1)));
        Gdiplus::Pen separator(separatorColor, separatorWidth);
        separator.SetStartCap(Gdiplus::LineCapFlat);
        separator.SetEndCap(Gdiplus::LineCapFlat);
        for (std::size_t i = 0; i + 1 < layout_.slots.size(); ++i) {
            const TabSlot& left = layout_.slots[i];
            const TabSlot& right = layout_.slots[i + 1];
            if (!left.visible || !right.visible) {
                continue;
            }
            const int index = static_cast<int>(i);
            if (index == activeIndex_ || index + 1 == activeIndex_ ||
                index == hoverIndex_ || index + 1 == hoverIndex_) {
                continue;
            }
            const float x =
                static_cast<float>((left.rect.right() + right.rect.left()) / 2) + 0.5f;
            const float top = static_cast<float>(left.rect.top()) + inset;
            const float bottom = static_cast<float>(left.rect.bottom()) - inset;
            if (bottom > top) {
                graphics.DrawLine(&separator, x, top, x, bottom);
            }
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

    // Nav sidebar toggle, pinned to the right end of the strip.
    const RECT navRect = navButtonRect();
    if (navHover_) {
        fillRoundedRect(graphics, navRect, static_cast<float>(theme_->px(6)), palette.tabHover);
    }
    drawNavToggleIcon(graphics, navRect, navVisible_, navHover_, palette, theme_->px(4));

    if (overflowLeft_) {
        drawScrollChevron(graphics, leftArrowRect(), true, leftArrowHover_, palette,
                          theme_->px(4));
    }
    if (overflowRight_) {
        drawScrollChevron(graphics, rightArrowRect(), false, rightArrowHover_, palette,
                          theme_->px(4));
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
        ::SelectObject(dc, theme_->smallFont());
        ::SetTextColor(dc, ToColorRef(selected || hovered ? palette.textPrimary
                                                          : palette.textSecondary));

        RECT textRect = toRect(slot.rect);
        textRect.left += theme_->px(10);
        textRect.right = slot.closeRect.empty()
                             ? static_cast<LONG>(slot.rect.x + slot.rect.w - theme_->px(10))
                             : static_cast<LONG>(slot.closeRect.x - theme_->px(2));
        if (textRect.right <= textRect.left) {
            continue;
        }

        // A tiny downward offset on the selected tab gives a tactile "pressed"
        // response without touching the horizontal text/close-icon gap.
        if (selected) {
            const LONG offset = theme_->px(1);
            textRect.top += offset;
            textRect.bottom += offset;
        }
        const wchar_t* title = item.title.empty() ? L"未命名" : item.title.c_str();
        ::DrawTextW(dc, title, -1, &textRect,
                    DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    ::RestoreDC(dc, saved);

    // Edge fades sit on top of tabs/text on the clipped sides.
    if (overflowLeft_ || overflowRight_) {
        Gdiplus::Graphics overlay(dc);
        overlay.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        const int top = theme_->px(5);
        const int bottom = std::max(top, static_cast<int>(client.bottom) - 1);
        const int fadeWidth = theme_->px(18);
        if (overflowLeft_) {
            const int x = arrowSize();
            RECT fade{x, top, x + fadeWidth, bottom};
            drawEdgeFade(overlay, fade, true, palette.windowBg);
        }
        if (overflowRight_) {
            const int x = tabAreaRight() - arrowSize();
            RECT fade{x - fadeWidth, top, x, bottom};
            drawEdgeFade(overlay, fade, false, palette.windowBg);
        }
        overlay.Flush();
    }
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
    if (hitNavButton(pt)) {
        if (callbacks_.onToggleNav) {
            callbacks_.onToggleNav();
        }
        return 0;
    }

    if (hitLeftArrow(pt)) {
        scrollTabsBy(-1);
        return 0;
    }
    if (hitRightArrow(pt)) {
        scrollTabsBy(1);
        return 0;
    }

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
    if (hitNavButton(pt) || hitLeftArrow(pt) || hitRightArrow(pt)) {
        return 0;
    }
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
    const bool nextNavHover = hitNavButton(pt);
    const bool nextLeftHover = hitLeftArrow(pt);
    const bool nextRightHover = hitRightArrow(pt);
    if (nextHover != hoverIndex_ || nextClose != closeHoverIndex_ ||
        nextNavHover != navHover_ || nextLeftHover != leftArrowHover_ ||
        nextRightHover != rightArrowHover_) {
        hoverIndex_ = nextHover;
        closeHoverIndex_ = nextClose;
        navHover_ = nextNavHover;
        leftArrowHover_ = nextLeftHover;
        rightArrowHover_ = nextRightHover;
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT TabStrip::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&)
{
    trackingMouse_ = false;
    if (hoverIndex_ != -1 || closeHoverIndex_ != -1 || navHover_ || leftArrowHover_ ||
        rightArrowHover_) {
        hoverIndex_ = -1;
        closeHoverIndex_ = -1;
        navHover_ = false;
        leftArrowHover_ = false;
        rightArrowHover_ = false;
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT TabStrip::OnMouseWheel(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (widths_.empty() || maxFirstVisible_ <= 0) {
        wheelDelta_ = 0;
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

    next = std::clamp(next, 0, maxFirstVisible_);
    if (next != firstVisible_) {
        firstVisible_ = next;
        relayout();
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT TabStrip::OnContextMenu(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    if (!callbacks_.onContextMenu) {
        return 0;
    }

    POINT screen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    int index = -1;
    if (screen.x == -1 && screen.y == -1) {
        // Keyboard invocation (Shift+F10 / menu key): anchor on the active tab.
        index = activeIndex_;
        if (index < 0 || static_cast<std::size_t>(index) >= layout_.slots.size() ||
            !layout_.slots[static_cast<std::size_t>(index)].visible) {
            return 0;
        }
        const RectI& rect = layout_.slots[static_cast<std::size_t>(index)].rect;
        screen = POINT{rect.x + rect.w / 2, rect.y + rect.h / 2};
        ClientToScreen(&screen);
    } else {
        POINT client = screen;
        ScreenToClient(&client);
        index = hitTab(client);
    }

    if (index >= 0) {
        callbacks_.onContextMenu(index, screen);
    }
    return 0;
}

LRESULT TabStrip::OnTestState(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    switch (wParam) {
    case 0:
        return (overflowLeft_ ? 1 : 0) | (overflowRight_ ? 2 : 0) | (navVisible_ ? 4 : 0);
    case 1:
        return firstVisible_;
    case 2:
        return maxFirstVisible_;
    default:
        return 0;
    }
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
    const bool interactive = hitClose(pt) >= 0 || hitNavButton(pt) || hitLeftArrow(pt) ||
                             hitRightArrow(pt);
    ::SetCursor(::LoadCursorW(nullptr, interactive ? IDC_HAND : IDC_ARROW));
    return TRUE;
}

} // namespace awm::ui
