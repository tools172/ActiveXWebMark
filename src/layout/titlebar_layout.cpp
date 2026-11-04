#include "titlebar_layout.h"

#include <algorithm>
#include <cwctype>

namespace awm::ui {

namespace {

bool isSpace(wchar_t ch)
{
    return std::iswspace(static_cast<wint_t>(ch)) != 0;
}

std::wstring trim(const std::wstring& text)
{
    std::size_t first = 0;
    while (first < text.size() && isSpace(text[first])) {
        ++first;
    }
    std::size_t last = text.size();
    while (last > first && isSpace(text[last - 1])) {
        --last;
    }
    return text.substr(first, last - first);
}

/// Removes Win32 ampersand markers. "&&" becomes a literal '&'; a single '&'
/// marks the following character as the mnemonic. The first mnemonic wins.
std::wstring stripAmpersands(const std::wstring& source, wchar_t& mnemonic)
{
    std::wstring text;
    text.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const wchar_t ch = source[i];
        if (ch != L'&') {
            text.push_back(ch);
            continue;
        }
        if (i + 1 < source.size() && source[i + 1] == L'&') {
            text.push_back(L'&');
            ++i;
        } else if (i + 1 < source.size()) {
            if (mnemonic == 0) {
                mnemonic = source[i + 1];
            }
            // The mnemonic character is still part of the visible label.
            text.push_back(source[i + 1]);
            ++i;
        }
        // A trailing lone '&' is dropped.
    }
    return text;
}

} // namespace

TitleBarLayout layoutTitleBar(const RectI& bar, const TitleBarMetrics& metrics,
                              const std::vector<int>& menuWidths)
{
    TitleBarLayout layout;
    layout.bar = bar;
    layout.menus.resize(menuWidths.size());

    if (bar.w <= 0 || bar.h <= 0) {
        return layout;
    }

    const auto clampSize = [&bar](int value) {
        return std::clamp(value, 0, bar.h);
    };

    const int iconSize = clampSize(metrics.iconSize);
    const int margin = std::max(0, metrics.iconMargin);
    layout.icon = RectI{bar.x + margin, bar.y + (bar.h - iconSize) / 2, iconSize, iconSize};

    int nextLeft = layout.icon.right() + margin;
    for (std::size_t i = 0; i < menuWidths.size(); ++i) {
        const int width = std::max(0, menuWidths[i]);
        layout.menus[i] = RectI{nextLeft, bar.y, width, bar.h};
        nextLeft += width + std::max(0, metrics.menuGap);
    }

    const int buttonWidth = std::clamp(metrics.buttonWidth, 0, bar.w);
    layout.closeButton =
        RectI{bar.right() - buttonWidth, bar.y, buttonWidth, bar.h};
    layout.maximizeButton =
        RectI{layout.closeButton.left() - buttonWidth, bar.y, buttonWidth, bar.h};
    layout.minimizeButton =
        RectI{layout.maximizeButton.left() - buttonWidth, bar.y, buttonWidth, bar.h};

    int titleLeft = std::clamp(nextLeft + margin, bar.x, bar.right());
    int titleRight = std::clamp(layout.minimizeButton.left() - margin, bar.x, bar.right());
    if (titleRight < titleLeft) {
        titleRight = titleLeft;
    }
    layout.title = RectI{titleLeft, bar.y, titleRight - titleLeft, bar.h};

    return layout;
}

TitleBarHit hitTestTitleBar(const TitleBarLayout& layout, int x, int y, int* menuIndex)
{
    if (menuIndex != nullptr) {
        *menuIndex = -1;
    }
    if (!rectContains(layout.bar, x, y)) {
        return TitleBarHit::None;
    }

    // Window buttons win over menu labels when a narrow bar makes them overlap.
    if (rectContains(layout.closeButton, x, y)) {
        return TitleBarHit::Close;
    }
    if (rectContains(layout.maximizeButton, x, y)) {
        return TitleBarHit::Maximize;
    }
    if (rectContains(layout.minimizeButton, x, y)) {
        return TitleBarHit::Minimize;
    }

    for (std::size_t i = 0; i < layout.menus.size(); ++i) {
        if (rectContains(layout.menus[i], x, y)) {
            if (menuIndex != nullptr) {
                *menuIndex = static_cast<int>(i);
            }
            return TitleBarHit::Menu;
        }
    }

    if (rectContains(layout.icon, x, y)) {
        return TitleBarHit::Icon;
    }

    return TitleBarHit::Caption;
}

MenuLabel parseMenuLabel(const std::wstring& raw)
{
    MenuLabel label;

    // Drop the accelerator column, then surrounding whitespace.
    std::wstring text = raw;
    const std::size_t tab = text.find(L'\t');
    if (tab != std::wstring::npos) {
        text.erase(tab);
    }
    text = trim(text);

    // Win32 convention for localized labels: "文件(&F)".
    if (text.size() >= 4 && text.back() == L')') {
        const std::size_t open = text.rfind(L'(');
        if (open != std::wstring::npos && text.size() - open == 4 &&
            text[open + 1] == L'&' && text[open + 3] == L')') {
            const wchar_t mnemonic = text[open + 2];
            const std::wstring prefix = text.substr(0, open);
            wchar_t prefixMnemonic = 0;
            label.text = trim(stripAmpersands(prefix, prefixMnemonic));
            label.mnemonic = mnemonic;
            return label;
        }
    }

    label.text = trim(stripAmpersands(text, label.mnemonic));
    return label;
}

} // namespace awm::ui
