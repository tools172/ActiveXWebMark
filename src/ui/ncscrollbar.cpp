#include "ncscrollbar.h"

#include <algorithm>
#include <unordered_map>

#include <commctrl.h>

namespace awm::ui {

namespace {

constexpr UINT_PTR kSubclassId = 0x41574D53; // 'AWMS'

std::unordered_map<HWND, ScrollbarColors>& colorsByWindow()
{
    static std::unordered_map<HWND, ScrollbarColors> colors;
    return colors;
}

bool highContrastEnabled()
{
    HIGHCONTRASTW info = {};
    info.cbSize = sizeof(info);
    if (::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(info), &info, 0)) {
        return (info.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
    return false;
}

void fillSolid(HDC dc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &rect, brush);
        ::DeleteObject(brush);
    }
}

void fillThumb(HDC dc, const RECT& rect, COLORREF color)
{
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 4 || height <= 4) {
        fillSolid(dc, rect, color);
        return;
    }

    HBRUSH brush = ::CreateSolidBrush(color);
    HPEN pen = ::CreatePen(PS_NULL, 0, color);
    if (brush == nullptr || pen == nullptr) {
        if (brush != nullptr) {
            ::DeleteObject(brush);
        }
        if (pen != nullptr) {
            ::DeleteObject(pen);
        }
        return;
    }

    HGDIOBJ oldBrush = ::SelectObject(dc, brush);
    HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::RoundRect(dc, rect.left + 2, rect.top + 2, rect.right - 2, rect.bottom - 2, 6, 6);
    ::SelectObject(dc, oldPen);
    ::SelectObject(dc, oldBrush);
    ::DeleteObject(pen);
    ::DeleteObject(brush);
}

void drawArrow(HDC dc, const RECT& rect, bool vertical, bool forward, COLORREF color)
{
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 2 || height <= 2) {
        return;
    }

    const int inset = std::max(3, (vertical ? height : width) / 4);
    const int cx = (rect.left + rect.right) / 2;
    const int cy = (rect.top + rect.bottom) / 2;

    POINT points[3] = {};
    if (vertical) {
        if (!forward) {
            points[0] = {cx, rect.top + inset};
            points[1] = {rect.left + inset, rect.bottom - inset};
            points[2] = {rect.right - inset, rect.bottom - inset};
        } else {
            points[0] = {cx, rect.bottom - inset};
            points[1] = {rect.left + inset, rect.top + inset};
            points[2] = {rect.right - inset, rect.top + inset};
        }
    } else {
        if (!forward) {
            points[0] = {rect.left + inset, cy};
            points[1] = {rect.right - inset, rect.top + inset};
            points[2] = {rect.right - inset, rect.bottom - inset};
        } else {
            points[0] = {rect.right - inset, cy};
            points[1] = {rect.left + inset, rect.top + inset};
            points[2] = {rect.left + inset, rect.bottom - inset};
        }
    }

    HBRUSH brush = ::CreateSolidBrush(color);
    HPEN pen = ::CreatePen(PS_NULL, 0, color);
    if (brush == nullptr || pen == nullptr) {
        if (brush != nullptr) {
            ::DeleteObject(brush);
        }
        if (pen != nullptr) {
            ::DeleteObject(pen);
        }
        return;
    }

    HGDIOBJ oldBrush = ::SelectObject(dc, brush);
    HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::Polygon(dc, points, 3);
    ::SelectObject(dc, oldPen);
    ::SelectObject(dc, oldBrush);
    ::DeleteObject(pen);
    ::DeleteObject(brush);
}

void paintBar(HDC dc, HWND hwnd, LONG id, const RECT& windowRect,
              const ScrollbarColors& colors)
{
    SCROLLBARINFO info = {};
    info.cbSize = sizeof(info);
    if (!::GetScrollBarInfo(hwnd, id, &info)) {
        return;
    }
    if ((info.rgstate[0] & STATE_SYSTEM_INVISIBLE) != 0) {
        return;
    }

    RECT bar = info.rcScrollBar;
    ::OffsetRect(&bar, -windowRect.left, -windowRect.top);
    if (bar.right <= bar.left || bar.bottom <= bar.top) {
        return;
    }

    const bool vertical = id == OBJID_VSCROLL;
    const int barLength =
        static_cast<int>(vertical ? bar.bottom - bar.top : bar.right - bar.left);
    const int button = std::clamp(info.dxyLineButton, 0, std::max(0, barLength / 2));

    // Track.
    fillSolid(dc, bar, colors.track);

    // Thumb. xyThumbTop/Bottom are offsets along the scrollbar.
    const int thumbStart = info.xyThumbTop;
    const int thumbEnd = info.xyThumbBottom;
    if (thumbEnd > thumbStart) {
        RECT thumb = bar;
        if (vertical) {
            thumb.top = bar.top + thumbStart;
            thumb.bottom = bar.top + thumbEnd;
        } else {
            thumb.left = bar.left + thumbStart;
            thumb.right = bar.left + thumbEnd;
        }
        fillThumb(dc, thumb, colors.thumb);
    }

    if (button <= 0) {
        return;
    }

    const bool unavailable = (info.rgstate[0] & STATE_SYSTEM_UNAVAILABLE) != 0;
    const COLORREF arrowColor =
        unavailable ? ::GetSysColor(COLOR_GRAYTEXT) : colors.arrow;

    // Button backgrounds and arrows.
    RECT first = bar;
    RECT second = bar;
    if (vertical) {
        first.bottom = bar.top + button;
        second.top = bar.bottom - button;
    } else {
        first.right = bar.left + button;
        second.left = bar.right - button;
    }
    fillSolid(dc, first, colors.track);
    fillSolid(dc, second, colors.track);
    drawArrow(dc, first, vertical, false, arrowColor);
    drawArrow(dc, second, vertical, true, arrowColor);
}

void paintScrollbars(HWND hwnd, const ScrollbarColors& colors)
{
    if (highContrastEnabled()) {
        return;
    }

    RECT windowRect = {};
    if (!::GetWindowRect(hwnd, &windowRect)) {
        return;
    }

    HDC dc = ::GetWindowDC(hwnd);
    if (dc == nullptr) {
        return;
    }
    paintBar(dc, hwnd, OBJID_VSCROLL, windowRect, colors);
    paintBar(dc, hwnd, OBJID_HSCROLL, windowRect, colors);
    ::ReleaseDC(hwnd, dc);
}

void invalidateFrame(HWND hwnd)
{
    ::RedrawWindow(hwnd, nullptr, nullptr,
                   RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
}

LRESULT CALLBACK scrollbarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR /*id*/, DWORD_PTR /*refData*/)
{
    if (msg == WM_NCDESTROY) {
        colorsByWindow().erase(hwnd);
        ::RemoveWindowSubclass(hwnd, &scrollbarSubclassProc, kSubclassId);
        return ::DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    if (msg == WM_NCPAINT) {
        const LRESULT result = ::DefSubclassProc(hwnd, msg, wParam, lParam);
        const auto it = colorsByWindow().find(hwnd);
        if (it != colorsByWindow().end()) {
            paintScrollbars(hwnd, it->second);
        }
        return result;
    }

    switch (msg) {
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_SIZE:
    case WM_THEMECHANGED:
    case WM_STYLECHANGED: {
        const LRESULT result = ::DefSubclassProc(hwnd, msg, wParam, lParam);
        if (colorsByWindow().find(hwnd) != colorsByWindow().end()) {
            invalidateFrame(hwnd);
        }
        return result;
    }
    default:
        break;
    }

    return ::DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace

void SetThemedScrollbar(HWND hwnd, const ScrollbarColors& colors)
{
    if (hwnd == nullptr || !::IsWindow(hwnd)) {
        return;
    }
    if (highContrastEnabled()) {
        RemoveThemedScrollbar(hwnd);
        return;
    }

    auto& colorsByHwnd = colorsByWindow();
    const auto it = colorsByHwnd.find(hwnd);
    if (it == colorsByHwnd.end()) {
        colorsByHwnd.emplace(hwnd, colors);
        if (!::SetWindowSubclass(hwnd, &scrollbarSubclassProc, kSubclassId, 0)) {
            colorsByHwnd.erase(hwnd);
            return;
        }
    } else {
        it->second = colors;
    }

    invalidateFrame(hwnd);
}

void RemoveThemedScrollbar(HWND hwnd)
{
    if (hwnd == nullptr) {
        return;
    }
    auto& colorsByHwnd = colorsByWindow();
    const auto it = colorsByHwnd.find(hwnd);
    if (it == colorsByHwnd.end()) {
        return;
    }
    colorsByHwnd.erase(it);
    ::RemoveWindowSubclass(hwnd, &scrollbarSubclassProc, kSubclassId);
    if (::IsWindow(hwnd)) {
        invalidateFrame(hwnd);
    }
}

} // namespace awm::ui
