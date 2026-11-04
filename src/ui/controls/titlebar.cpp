#include "titlebar.h"

#include <algorithm>
#include <cwctype>

namespace awm::ui {

namespace {

constexpr int kTitleBarHeight = 36;
constexpr int kIconSize = 16;
constexpr int kIconMargin = 10;
constexpr int kMenuGap = 2;
constexpr int kMenuPaddingX = 10;
constexpr int kButtonWidth = 46;
constexpr int kMenuHoverInsetY = 4;
constexpr int kMenuCornerRadius = 4;
constexpr int kGlyphSize = 10;

RECT toRect(const RectI& rect)
{
    return RECT{rect.x, rect.y, rect.right(), rect.bottom()};
}

void fillRect(HDC dc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &rect, brush);
        ::DeleteObject(brush);
    }
}

void fillRoundRect(HDC dc, const RECT& rect, int radius, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    HGDIOBJ oldBrush = ::SelectObject(dc, brush);
    HGDIOBJ oldPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
    ::RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);
    ::SelectObject(dc, oldPen);
    ::SelectObject(dc, oldBrush);
    ::DeleteObject(brush);
}

} // namespace

void TitleBar::SetTheme(const Theme* theme)
{
    theme_ = theme;
}

void TitleBar::SetMenus(std::vector<MenuLabel> menus)
{
    menus_ = std::move(menus);
    hoveredMenu_ = -1;
    pressedMenu_ = -1;
    highlightedMenu_ = -1;
    openMenu_ = -1;
    if (hoveredPart_ == TitleBarHit::Menu) {
        hoveredPart_ = TitleBarHit::None;
    }
    if (pressedPart_ == TitleBarHit::Menu) {
        pressedPart_ = TitleBarHit::None;
    }
}

void TitleBar::SetTitle(std::wstring title)
{
    title_ = std::move(title);
}

void TitleBar::SetActive(bool active)
{
    active_ = active;
}

void TitleBar::SetMaximized(bool maximized)
{
    maximized_ = maximized;
}

void TitleBar::SetMenuMode(bool enabled)
{
    menuMode_ = enabled;
}

void TitleBar::SetHighlightedMenu(int index)
{
    highlightedMenu_ = std::clamp(index, -1, menuCount() - 1);
}

void TitleBar::SetOpenMenu(int index)
{
    openMenu_ = std::clamp(index, -1, menuCount() - 1);
}

int TitleBar::height() const
{
    return px(kTitleBarHeight);
}

TitleBarMetrics TitleBar::metrics() const
{
    TitleBarMetrics value;
    value.height = px(kTitleBarHeight);
    value.iconSize = px(kIconSize);
    value.iconMargin = px(kIconMargin);
    value.menuGap = px(kMenuGap);
    value.menuPaddingX = px(kMenuPaddingX);
    value.buttonWidth = px(kButtonWidth);
    return value;
}

int TitleBar::px(int logicalPx) const
{
    return theme_ != nullptr ? theme_->px(logicalPx) : logicalPx;
}

std::vector<int> TitleBar::measureMenuWidths() const
{
    std::vector<int> widths;
    widths.reserve(menus_.size());
    if (menus_.empty()) {
        return widths;
    }

    HDC dc = ::GetDC(nullptr);
    HGDIOBJ oldFont = nullptr;
    if (dc != nullptr && theme_ != nullptr) {
        oldFont = ::SelectObject(dc, theme_->uiFont());
    }

    for (const MenuLabel& menu : menus_) {
        int textWidth = static_cast<int>(menu.text.size()) * px(8);
        if (dc != nullptr && theme_ != nullptr) {
            SIZE size = {};
            if (::GetTextExtentPoint32W(dc, menu.text.c_str(),
                                        static_cast<int>(menu.text.size()), &size) != FALSE) {
                textWidth = size.cx;
            }
        }
        widths.push_back(textWidth + 2 * px(kMenuPaddingX));
    }

    if (dc != nullptr) {
        if (oldFont != nullptr) {
            ::SelectObject(dc, oldFont);
        }
        ::ReleaseDC(nullptr, dc);
    }
    return widths;
}

int TitleBar::Layout(int width)
{
    const int barHeight = height();
    layout_ = layoutTitleBar(RectI{0, 0, std::max(0, width), barHeight}, metrics(),
                             measureMenuWidths());
    return barHeight;
}

const MenuLabel& TitleBar::menuAt(int index) const
{
    static const MenuLabel kEmpty;
    if (index < 0 || index >= menuCount()) {
        return kEmpty;
    }
    return menus_[static_cast<std::size_t>(index)];
}

int TitleBar::mnemonicMenu(wchar_t mnemonic) const
{
    if (mnemonic == 0) {
        return -1;
    }
    const wchar_t wanted = static_cast<wchar_t>(std::towupper(static_cast<wint_t>(mnemonic)));
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        if (menus_[i].mnemonic != 0 &&
            static_cast<wchar_t>(std::towupper(static_cast<wint_t>(menus_[i].mnemonic))) ==
                wanted) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

TitleBarHit TitleBar::hitTest(POINT client, int* menuIndex) const
{
    return hitTestTitleBar(layout_, client.x, client.y, menuIndex);
}

RECT TitleBar::menuRect(int index) const
{
    if (index < 0 || index >= static_cast<int>(layout_.menus.size())) {
        return RECT{0, 0, 0, 0};
    }
    return toRect(layout_.menus[static_cast<std::size_t>(index)]);
}

void TitleBar::SetHoverForTest(TitleBarHit part, int menuIndex)
{
    hoveredPart_ = part;
    hoveredMenu_ = (part == TitleBarHit::Menu && menuCount() > 0)
                       ? std::clamp(menuIndex, 0, menuCount() - 1)
                       : -1;
}

void TitleBar::resolveHit(POINT client, TitleBarHit& part, int& menu) const
{
    menu = -1;
    part = hitTest(client, &menu);
    if (part != TitleBarHit::Menu) {
        menu = -1;
    }
}

bool TitleBar::OnMouseMove(POINT client)
{
    TitleBarHit part = TitleBarHit::None;
    int menu = -1;
    resolveHit(client, part, menu);

    if (part == hoveredPart_ && menu == hoveredMenu_) {
        return false;
    }
    hoveredPart_ = part;
    hoveredMenu_ = menu;
    return true;
}

bool TitleBar::OnMouseLeave()
{
    if (hoveredPart_ == TitleBarHit::None && hoveredMenu_ == -1) {
        return false;
    }
    hoveredPart_ = TitleBarHit::None;
    hoveredMenu_ = -1;
    return true;
}

bool TitleBar::OnLButtonDown(POINT client)
{
    OnMouseMove(client);

    TitleBarHit part = TitleBarHit::None;
    int menu = -1;
    resolveHit(client, part, menu);

    const TitleBarHit previousPart = pressedPart_;
    const int previousMenu = pressedMenu_;
    switch (part) {
    case TitleBarHit::Menu:
    case TitleBarHit::Icon:
    case TitleBarHit::Minimize:
    case TitleBarHit::Maximize:
    case TitleBarHit::Close:
        pressedPart_ = part;
        pressedMenu_ = part == TitleBarHit::Menu ? menu : -1;
        break;
    default:
        pressedPart_ = TitleBarHit::None;
        pressedMenu_ = -1;
        break;
    }
    return previousPart != pressedPart_ || previousMenu != pressedMenu_;
}

TitleBarEvent TitleBar::OnLButtonUp(POINT client)
{
    TitleBarHit part = TitleBarHit::None;
    int menu = -1;
    resolveHit(client, part, menu);

    TitleBarEvent event;
    if (pressedPart_ != TitleBarHit::None && part == pressedPart_ &&
        (pressedPart_ != TitleBarHit::Menu || menu == pressedMenu_)) {
        switch (pressedPart_) {
        case TitleBarHit::Menu:
            event.action = TitleBarAction::OpenMenu;
            event.menuIndex = pressedMenu_;
            break;
        case TitleBarHit::Icon:
            event.action = TitleBarAction::ShowSystemMenu;
            break;
        case TitleBarHit::Minimize:
            event.action = TitleBarAction::Minimize;
            break;
        case TitleBarHit::Maximize:
            event.action = TitleBarAction::ToggleMaximize;
            break;
        case TitleBarHit::Close:
            event.action = TitleBarAction::Close;
            break;
        default:
            break;
        }
    }

    pressedPart_ = TitleBarHit::None;
    pressedMenu_ = -1;
    OnMouseMove(client);
    return event;
}

void TitleBar::CancelPress()
{
    pressedPart_ = TitleBarHit::None;
    pressedMenu_ = -1;
}

void TitleBar::Paint(HDC dc) const
{
    if (theme_ == nullptr || dc == nullptr || layout_.bar.w <= 0 || layout_.bar.h <= 0) {
        return;
    }

    const Palette& palette = theme_->palette();
    const RECT bar = toRect(layout_.bar);
    fillRect(dc, bar, ToColorRef(active_ ? palette.titleBarBg : palette.titleBarInactive));

    HGDIOBJ oldFont = ::SelectObject(dc, theme_->uiFont());
    const int oldBkMode = ::SetBkMode(dc, TRANSPARENT);

    if (icon_ != nullptr && !layout_.icon.empty()) {
        ::DrawIconEx(dc, layout_.icon.x, layout_.icon.y, icon_, layout_.icon.w,
                     layout_.icon.h, 0, nullptr, DI_NORMAL);
    }

    for (int i = 0; i < menuCount(); ++i) {
        paintMenu(dc, i);
    }
    paintTitle(dc);
    paintButton(dc, layout_.minimizeButton, TitleBarHit::Minimize);
    paintButton(dc, layout_.maximizeButton, TitleBarHit::Maximize);
    paintButton(dc, layout_.closeButton, TitleBarHit::Close);

    const int dividerThickness = std::max(1, px(1));
    const RECT divider{bar.left, bar.bottom - dividerThickness, bar.right, bar.bottom};
    fillRect(dc, divider, ToColorRef(palette.divider));

    ::SetBkMode(dc, oldBkMode);
    ::SelectObject(dc, oldFont);
}

void TitleBar::paintMenu(HDC dc, int index) const
{
    const Palette& palette = theme_->palette();
    const RectI& rect = layout_.menus[static_cast<std::size_t>(index)];
    if (rect.empty()) {
        return;
    }

    const bool open = index == openMenu_ && openMenu_ >= 0;
    const bool highlighted = index == highlightedMenu_ && highlightedMenu_ >= 0;
    const bool hovered = index == hoveredMenu_ && hoveredPart_ == TitleBarHit::Menu;

    if (open || highlighted || hovered) {
        RECT background = toRect(rect);
        background.top += px(kMenuHoverInsetY);
        background.bottom -= px(kMenuHoverInsetY);
        const COLORREF color =
            ToColorRef(open || highlighted ? palette.accentSoft : palette.tabHover);
        fillRoundRect(dc, background, std::max(1, px(kMenuCornerRadius)), color);
    }

    paintMenuText(dc, menus_[static_cast<std::size_t>(index)], rect);
}

void TitleBar::paintMenuText(HDC dc, const MenuLabel& menu, const RectI& rect) const
{
    if (menu.text.empty()) {
        return;
    }
    const Palette& palette = theme_->palette();
    ::SetTextColor(dc, ToColorRef(active_ ? palette.textPrimary : palette.textSecondary));

    SIZE size = {};
    ::GetTextExtentPoint32W(dc, menu.text.c_str(), static_cast<int>(menu.text.size()), &size);
    int x = rect.x + (rect.w - size.cx) / 2;
    int y = rect.y + (rect.h - size.cy) / 2;
    if (x < rect.x) {
        x = rect.x;
    }
    ::TextOutW(dc, x, y, menu.text.c_str(), static_cast<int>(menu.text.size()));

    // Mnemonic underline (the letter only exists in ASCII labels such as "&File";
    // localized labels like 文件(&F) have no matching glyph, so they stay clean).
    if (!menuMode_ || menu.mnemonic == 0) {
        return;
    }
    const wchar_t wanted =
        static_cast<wchar_t>(std::towlower(static_cast<wint_t>(menu.mnemonic)));
    for (std::size_t i = 0; i < menu.text.size(); ++i) {
        if (static_cast<wchar_t>(std::towlower(static_cast<wint_t>(menu.text[i]))) != wanted) {
            continue;
        }
        SIZE prefix = {};
        SIZE letter = {};
        ::GetTextExtentPoint32W(dc, menu.text.c_str(), static_cast<int>(i), &prefix);
        ::GetTextExtentPoint32W(dc, menu.text.c_str() + i, 1, &letter);
        const int thickness = std::max(1, px(1));
        const int underlineY = y + size.cy - px(2);
        const RECT underline{x + prefix.cx, underlineY, x + prefix.cx + letter.cx,
                             underlineY + thickness};
        fillRect(dc, underline, ToColorRef(active_ ? palette.textPrimary : palette.textSecondary));
        break;
    }
}

void TitleBar::paintTitle(HDC dc) const
{
    if (title_.empty() || layout_.title.empty()) {
        return;
    }
    const Palette& palette = theme_->palette();
    ::SetTextColor(dc, ToColorRef(active_ ? palette.textPrimary : palette.textSecondary));
    RECT rect = toRect(layout_.title);
    rect.left += px(4);
    rect.right -= px(4);
    if (rect.right <= rect.left) {
        return;
    }
    ::DrawTextW(dc, title_.c_str(), -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void TitleBar::paintButton(HDC dc, const RectI& rect, TitleBarHit part) const
{
    if (rect.empty()) {
        return;
    }
    const Palette& palette = theme_->palette();
    const bool hovered = hoveredPart_ == part;
    const bool pressed = pressedPart_ == part;
    const RECT button = toRect(rect);

    if (part == TitleBarHit::Close) {
        if (pressed) {
            fillRect(dc, button, ToColorRef(palette.closePressed));
        } else if (hovered) {
            fillRect(dc, button, ToColorRef(palette.closeHover));
        }
    } else if (pressed) {
        fillRect(dc, button, ToColorRef(palette.buttonPressed));
    } else if (hovered) {
        fillRect(dc, button, ToColorRef(palette.buttonHover));
    }

    const bool inverted = part == TitleBarHit::Close && (hovered || pressed);
    const COLORREF glyphColor =
        inverted ? RGB(255, 255, 255)
                 : ToColorRef(active_ ? palette.textPrimary : palette.textSecondary);
    paintGlyph(dc, rect, part, glyphColor);
}

void TitleBar::paintGlyph(HDC dc, const RectI& rect, TitleBarHit part, COLORREF color) const
{
    const int unit = std::max(6, px(kGlyphSize));
    const int half = unit / 2;
    const int centerX = rect.x + rect.w / 2;
    const int centerY = rect.y + rect.h / 2;
    const int left = centerX - half;
    const int top = centerY - half;
    const int right = left + unit - 1;
    const int bottom = top + unit - 1;

    HPEN pen = ::CreatePen(PS_SOLID, std::max(1, px(1)), color);
    if (pen == nullptr) {
        return;
    }
    HGDIOBJ oldPen = ::SelectObject(dc, pen);
    HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(HOLLOW_BRUSH));

    switch (part) {
    case TitleBarHit::Minimize:
        ::MoveToEx(dc, left, centerY, nullptr);
        ::LineTo(dc, right, centerY);
        break;
    case TitleBarHit::Maximize:
        if (maximized_) {
            const int offset = std::max(2, px(2));
            const int box = std::max(2, unit - offset);
            // Back window (top-right), then the front window (bottom-left).
            ::Rectangle(dc, left + offset, top, left + offset + box - 1, top + box - 1);
            ::Rectangle(dc, left, top + offset, left + box - 1, top + offset + box - 1);
        } else {
            ::Rectangle(dc, left, top, right, bottom);
        }
        break;
    case TitleBarHit::Close:
        ::MoveToEx(dc, left, top, nullptr);
        ::LineTo(dc, right, bottom);
        ::MoveToEx(dc, left, bottom, nullptr);
        ::LineTo(dc, right, top);
        break;
    default:
        break;
    }

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

} // namespace awm::ui
