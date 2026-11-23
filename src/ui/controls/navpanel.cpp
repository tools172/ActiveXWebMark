#include "controls/navpanel.h"

#include <algorithm>
#include <utility>

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include "layout/navrows.h"

namespace awm::ui {

namespace {

/// Tooltip delay after the pointer settles on a truncated heading, matching
/// the native tree tooltip's pace.
constexpr UINT_PTR kHoverTimerId = 2;
constexpr UINT kTooltipDelayMs = 450;
constexpr UINT_PTR kTreeSubclassId = 1;

void fillSolid(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &rc, brush);
        ::DeleteObject(brush);
    }
}

int measureTextWidth(HDC dc, const std::wstring& text)
{
    if (text.empty()) {
        return 0;
    }
    SIZE size = {};
    if (!::GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size)) {
        return 0;
    }
    return size.cx;
}

/// Greedily wraps one paragraph. The tooltip's own wrapping only breaks at
/// spaces, so long Chinese headings stay on a single very wide line; wrapping
/// here lets us place the tip fully on screen.
std::wstring wrapParagraph(HDC dc, const std::wstring& paragraph, int maxWidth)
{
    std::wstring wrapped;
    std::wstring line;
    std::size_t breakAt = std::wstring::npos;

    for (const wchar_t ch : paragraph) {
        line.push_back(ch);
        if (line.size() <= 1 || measureTextWidth(dc, line) <= maxWidth) {
            if (ch == L' ') {
                breakAt = line.size() - 1;
            }
            continue;
        }

        line.pop_back();
        if (breakAt != std::wstring::npos && breakAt > 0) {
            std::wstring rest = line.substr(breakAt + 1);
            line.resize(breakAt);
            wrapped += line;
            wrapped.push_back(L'\n');
            line = std::move(rest);
            while (!line.empty() && line.front() == L' ') {
                line.erase(line.begin());
            }
            if (!(line.empty() && ch == L' ')) {
                line.push_back(ch);
            }
        } else {
            wrapped += line;
            wrapped.push_back(L'\n');
            line.assign(1, ch);
        }
        breakAt = line.find_last_of(L' ');
    }

    wrapped += line;
    return wrapped;
}

std::wstring wrapTooltipText(HDC dc, const std::wstring& text, int maxWidth)
{
    std::wstring wrapped;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(L'\n', start);
        std::wstring paragraph =
            text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!paragraph.empty() && paragraph.back() == L'\r') {
            paragraph.pop_back();
        }
        wrapped += wrapParagraph(dc, paragraph, maxWidth);
        if (end == std::wstring::npos) {
            break;
        }
        wrapped.push_back(L'\n');
        start = end + 1;
    }
    return wrapped;
}

} // namespace

bool NavPanel::Create(HWND parent, const RECT& rc, UINT id)
{
    RECT rect = rc;
    if (CWindowImpl<NavPanel>::Create(parent, rect, nullptr,
                                      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, id) ==
        nullptr) {
        return false;
    }

    // Long headings are clipped instead of scrolling horizontally: selecting an
    // item calls TreeView_EnsureVisible, which would otherwise shift the tree
    // sideways on every click. The native tree tooltip is disabled because it
    // can grow past the right screen edge (it only breaks lines at spaces);
    // NavPanel shows its own wrapped tooltip instead.
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS |
                        TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_TRACKSELECT |
                        TVS_NOHSCROLL | TVS_NOTOOLTIPS | TVS_DISABLEDRAGDROP;
    RECT treeRect = {0, 0, 0, 0};
    if (tree_.Create(m_hWnd, treeRect, nullptr, style, 0, kTreeId) == nullptr) {
        return false;
    }

    if (tooltip_.Create(m_hWnd, nullptr, nullptr,
                        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX) != nullptr) {
        toolInfo_ = {};
        toolInfo_.cbSize = sizeof(toolInfo_);
        toolInfo_.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        toolInfo_.hwnd = tree_.m_hWnd;
        toolInfo_.uId = 1;
        toolInfo_.lpszText = const_cast<LPWSTR>(L"");
        ::SendMessageW(tooltip_.m_hWnd, TTM_ADDTOOLW, 0,
                       reinterpret_cast<LPARAM>(&toolInfo_));
        // The text is pre-wrapped by showTooltip(); never re-wrap it.
        ::SendMessageW(tooltip_.m_hWnd, TTM_SETMAXTIPWIDTH, 0, -1);
    }
    ::SetWindowSubclass(tree_.m_hWnd, &NavPanel::TreeSubclassProc, kTreeSubclassId,
                        reinterpret_cast<DWORD_PTR>(this));

    ::SetWindowTheme(tree_.m_hWnd, L"Explorer", nullptr);
    TreeView_SetExtendedStyle(tree_.m_hWnd, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    layoutChildren();
    applyTheme();
    return true;
}

void NavPanel::SetTheme(const Theme* theme)
{
    dismissHover();
    theme_ = theme;
    applyTheme();
    layoutChildren();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void NavPanel::SetItems(std::vector<Item> items)
{
    dismissHover();
    items_ = std::move(items);
    handles_.assign(items_.size(), nullptr);

    if (!tree_.IsWindow()) {
        if (IsWindow()) {
            Invalidate(FALSE);
        }
        return;
    }

    suppressSelection_ = true;
    TreeView_DeleteAllItems(tree_.m_hWnd);

    if (!items_.empty()) {
        std::vector<int> levels;
        levels.reserve(items_.size());
        for (const Item& item : items_) {
            levels.push_back(item.level);
        }

        const std::vector<NavRow> rows = buildNavRows(levels);
        for (const NavRow& row : rows) {
            TVINSERTSTRUCTW insert = {};
            insert.hParent =
                row.parent >= 0 ? handles_[static_cast<std::size_t>(row.parent)] : TVI_ROOT;
            insert.hInsertAfter = TVI_LAST;
            insert.item.mask = TVIF_TEXT | TVIF_PARAM;
            insert.item.pszText =
                const_cast<LPWSTR>(items_[static_cast<std::size_t>(row.index)].text.c_str());
            insert.item.lParam = static_cast<LPARAM>(row.index);
            handles_[static_cast<std::size_t>(row.index)] =
                TreeView_InsertItem(tree_.m_hWnd, &insert);
        }

        // Expand every node that has children so the whole outline is visible
        // by default; users can collapse individual sections afterwards.
        for (const NavRow& row : rows) {
            if (row.hasChildren) {
                TreeView_Expand(tree_.m_hWnd, handles_[static_cast<std::size_t>(row.index)],
                                TVE_EXPAND);
            }
        }
    }

    suppressSelection_ = false;
    tree_.ShowWindow(items_.empty() ? SW_HIDE : SW_SHOW);
    // The themed scrollbar overlay needs a frame repaint after the item range
    // changed; the tree repaints its scrollbar directly otherwise.
    ::RedrawWindow(tree_.m_hWnd, nullptr, nullptr,
                   RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
    Invalidate(FALSE);
}

void NavPanel::SetSelected(std::size_t index)
{
    if (index >= handles_.size() || handles_[index] == nullptr || !tree_.IsWindow()) {
        return;
    }

    dismissHover();
    suppressSelection_ = true;
    TreeView_SelectItem(tree_.m_hWnd, handles_[index]);
    TreeView_EnsureVisible(tree_.m_hWnd, handles_[index]);
    suppressSelection_ = false;
    ::RedrawWindow(tree_.m_hWnd, nullptr, nullptr,
                   RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
}

int NavPanel::headerHeight() const
{
    return theme_ != nullptr ? theme_->px(36) : 36;
}

void NavPanel::SetCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

void NavPanel::OnDpiChanged()
{
    if (!IsWindow()) {
        return;
    }
    dismissHover();
    applyTheme();
    layoutChildren();
    Invalidate(FALSE);
}

void NavPanel::FocusTree() const
{
    if (tree_.IsWindow()) {
        ::SetFocus(tree_.m_hWnd);
    }
}

void NavPanel::applyTheme()
{
    if (theme_ == nullptr || !tree_.IsWindow()) {
        return;
    }

    const Palette& palette = theme_->palette();
    ::SendMessageW(tree_.m_hWnd, WM_SETFONT,
                   reinterpret_cast<WPARAM>(theme_->uiFont()), TRUE);
    // The tree uses the sidebar tint rather than the document surface, so the
    // navigation panel reads as chrome instead of another white page.
    TreeView_SetBkColor(tree_.m_hWnd, ToColorRef(palette.windowBg));
    TreeView_SetTextColor(tree_.m_hWnd, ToColorRef(palette.textPrimary));
    TreeView_SetLineColor(tree_.m_hWnd, ToColorRef(palette.divider));
    TreeView_SetItemHeight(tree_.m_hWnd, theme_->px(24));
    TreeView_SetIndent(tree_.m_hWnd, theme_->px(14));

    if (tooltip_.IsWindow()) {
        ::SendMessageW(tooltip_.m_hWnd, WM_SETFONT,
                       reinterpret_cast<WPARAM>(theme_->uiFont()), TRUE);
        RECT margins{theme_->px(10), theme_->px(7), theme_->px(10), theme_->px(7)};
        ::SendMessageW(tooltip_.m_hWnd, TTM_SETMARGIN, 0,
                       reinterpret_cast<LPARAM>(&margins));
        ::SendMessageW(tooltip_.m_hWnd, TTM_SETTIPBKCOLOR, 0,
                       static_cast<LPARAM>(ToColorRef(palette.surface)));
        ::SendMessageW(tooltip_.m_hWnd, TTM_SETTIPTEXTCOLOR, 0,
                       static_cast<LPARAM>(ToColorRef(palette.textPrimary)));
    }
}

void NavPanel::layoutChildren()
{
    dismissHover();
    if (!tree_.IsWindow()) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    const int header = headerHeight();
    tree_.MoveWindow(0, header, client.right,
                     std::max(0, static_cast<int>(client.bottom) - header), TRUE);
}

LRESULT NavPanel::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);
    const Palette palette = theme_ != nullptr ? theme_->palette() : Palette{};

    fillSolid(dc, client, ToColorRef(palette.windowBg));

    const int header = headerHeight();
    RECT headerRect{client.left, client.top, client.right, header};
    fillSolid(dc, headerRect, ToColorRef(palette.navBg));

    RECT divider{client.left, header - 1, client.right, header};
    if (header > 0) {
        fillSolid(dc, divider, ToColorRef(palette.divider));
    }

    const int saved = ::SaveDC(dc);
    ::SetBkMode(dc, TRANSPARENT);
    ::SelectObject(dc, theme_ != nullptr ? theme_->uiFontSemibold() : GetStockObject(DEFAULT_GUI_FONT));
    ::SetTextColor(dc, ToColorRef(palette.textSecondary));
    RECT titleRect{theme_ != nullptr ? theme_->px(14) : 14, 0,
                   client.right - (theme_ != nullptr ? theme_->px(14) : 14), header};
    ::DrawTextW(dc, L"目录", -1, &titleRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    if (items_.empty()) {
        ::SelectObject(dc, theme_ != nullptr ? theme_->smallFont() : GetStockObject(DEFAULT_GUI_FONT));
        ::SetTextColor(dc, ToColorRef(palette.textSecondary));
        RECT emptyRect{client.left, header, client.right, client.bottom};
        ::DrawTextW(dc, L"没有找到标题", -1, &emptyRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    ::RestoreDC(dc, saved);

    EndPaint(&ps);
    return 0;
}

LRESULT NavPanel::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT NavPanel::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    layoutChildren();
    Invalidate(FALSE);
    return 0;
}

LRESULT NavPanel::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (wParam != kHoverTimerId) {
        return 0;
    }
    ::KillTimer(m_hWnd, kHoverTimerId);
    hoverTimerArmed_ = false;

    if (hoverIndex_ < 0 || static_cast<std::size_t>(hoverIndex_) >= handles_.size() ||
        handles_[static_cast<std::size_t>(hoverIndex_)] == nullptr) {
        hoverIndex_ = -1;
        return 0;
    }

    // The pointer may have moved within the row, or the tree may have scrolled
    // while the timer was pending. Only show when the same item is still under
    // the cursor.
    POINT cursor = {};
    ::GetCursorPos(&cursor);
    POINT client = cursor;
    ::ScreenToClient(tree_.m_hWnd, &client);
    const HTREEITEM item = handles_[static_cast<std::size_t>(hoverIndex_)];
    if (itemIndexAt(client) != hoverIndex_ || !isLabelTruncated(item)) {
        hoverIndex_ = -1;
        return 0;
    }

    showTooltip(hoverIndex_);
    return 0;
}

LRESULT NavPanel::OnShowWindow(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (wParam == FALSE) {
        dismissHover();
    }
    return 0;
}

LRESULT CALLBACK NavPanel::TreeSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                            LPARAM lParam, UINT_PTR, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<NavPanel*>(refData);
    if (self != nullptr) {
        self->onTreeMessage(hwnd, message, wParam, lParam);
    }
    if (message == WM_NCDESTROY) {
        ::RemoveWindowSubclass(hwnd, &NavPanel::TreeSubclassProc, kTreeSubclassId);
    }
    return ::DefSubclassProc(hwnd, message, wParam, lParam);
}

void NavPanel::onTreeMessage(HWND tree, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    switch (message) {
    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            trackingMouse_ = true;
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = tree;
            ::TrackMouseEvent(&track);
        }

        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int index = itemIndexAt(point);
        if (index != hoverIndex_) {
            dismissHover();
            if (index >= 0) {
                hoverIndex_ = index;
                if (isLabelTruncated(handles_[static_cast<std::size_t>(index)])) {
                    armHover();
                }
            }
        }
        break;
    }
    case WM_MOUSELEAVE:
        trackingMouse_ = false;
        dismissHover();
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_KEYDOWN:
    case WM_KILLFOCUS:
    case WM_NCDESTROY:
        dismissHover();
        break;
    default:
        break;
    }
}

int NavPanel::itemIndexAt(POINT clientPoint) const
{
    if (!tree_.IsWindow()) {
        return -1;
    }

    TVHITTESTINFO hit = {};
    hit.pt = clientPoint;
    const HTREEITEM item = TreeView_HitTest(tree_.m_hWnd, &hit);
    if (item == nullptr || (hit.flags & TVHT_ONITEM) == 0) {
        return -1;
    }

    TVITEMW info = {};
    info.mask = TVIF_PARAM;
    info.hItem = item;
    if (::SendMessageW(tree_.m_hWnd, TVM_GETITEMW, 0,
                       reinterpret_cast<LPARAM>(&info)) == 0) {
        return -1;
    }

    const int index = static_cast<int>(info.lParam);
    if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) {
        return -1;
    }
    return index;
}

bool NavPanel::isLabelTruncated(HTREEITEM item) const
{
    if (item == nullptr || !tree_.IsWindow()) {
        return false;
    }

    RECT label = {};
    if (!TreeView_GetItemRect(tree_.m_hWnd, item, &label, TRUE)) {
        return false;
    }
    RECT client = {};
    tree_.GetClientRect(&client);
    return label.right > client.right;
}

void NavPanel::armHover()
{
    if (hoverTimerArmed_) {
        return;
    }
    if (::SetTimer(m_hWnd, kHoverTimerId, kTooltipDelayMs, nullptr) != 0) {
        hoverTimerArmed_ = true;
    }
}

void NavPanel::dismissHover()
{
    if (hoverTimerArmed_) {
        ::KillTimer(m_hWnd, kHoverTimerId);
        hoverTimerArmed_ = false;
    }
    hoverIndex_ = -1;
    hideTooltip();
}

void NavPanel::showTooltip(int index)
{
    if (!tooltip_.IsWindow() || index < 0 ||
        static_cast<std::size_t>(index) >= items_.size() ||
        static_cast<std::size_t>(index) >= handles_.size() ||
        handles_[static_cast<std::size_t>(index)] == nullptr) {
        return;
    }

    const HTREEITEM item = handles_[static_cast<std::size_t>(index)];
    RECT label = {};
    if (!TreeView_GetItemRect(tree_.m_hWnd, item, &label, TRUE)) {
        return;
    }

    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    const HMONITOR monitorHandle =
        ::MonitorFromWindow(tree_.m_hWnd, MONITOR_DEFAULTTONEAREST);
    if (!::GetMonitorInfoW(monitorHandle, &monitor)) {
        return;
    }
    const RECT work = monitor.rcWork;

    const int padX = theme_ != nullptr ? theme_->px(10) : 10;
    const int padY = theme_ != nullptr ? theme_->px(7) : 7;
    const int edge = theme_ != nullptr ? theme_->px(8) : 8;
    const int rowGap = theme_ != nullptr ? theme_->px(6) : 6;

    // Keep the tip close to the heading: below its row, aligned with the text
    // start, and clamped to the monitor work area.
    int maxTextWidth = theme_ != nullptr ? theme_->px(420) : 420;
    const int workWidth = (work.right - work.left) - (edge * 2) - (padX * 2);
    if (workWidth > 0) {
        maxTextWidth = std::min(maxTextWidth, workWidth);
    }
    maxTextWidth = std::max(maxTextWidth, theme_ != nullptr ? theme_->px(80) : 80);

    HDC dc = ::GetDC(m_hWnd);
    if (dc == nullptr) {
        return;
    }
    HGDIOBJ oldFont = ::SelectObject(dc, tooltipFont());
    tooltipText_ =
        wrapTooltipText(dc, items_[static_cast<std::size_t>(index)].text, maxTextWidth);
    RECT calc = {};
    ::DrawTextW(dc, tooltipText_.c_str(), -1, &calc, DT_CALCRECT | DT_NOPREFIX);
    ::SelectObject(dc, oldFont);
    ::ReleaseDC(m_hWnd, dc);

    toolInfo_.lpszText = tooltipText_.data();
    ::SendMessageW(tooltip_.m_hWnd, TTM_UPDATETIPTEXTW, 0,
                   reinterpret_cast<LPARAM>(&toolInfo_));

    const int tipWidth = calc.right + (padX * 2);
    const int tipHeight = calc.bottom + (padY * 2);

    POINT itemTopLeft{label.left, label.top};
    POINT itemBottom{label.left, label.bottom};
    ::ClientToScreen(tree_.m_hWnd, &itemTopLeft);
    ::ClientToScreen(tree_.m_hWnd, &itemBottom);

    int x = itemTopLeft.x;
    if (x + tipWidth > work.right - edge) {
        x = work.right - edge - tipWidth;
    }
    if (x < work.left + edge) {
        x = work.left + edge;
    }

    // Below the row by default; flip above it when there is no room left.
    int y = itemBottom.y + rowGap;
    if (y + tipHeight > work.bottom - edge) {
        y = itemTopLeft.y - rowGap - tipHeight;
    }
    if (y < work.top + edge) {
        y = work.top + edge;
    }
    if (y + tipHeight > work.bottom - edge) {
        y = std::max(work.top + edge, work.bottom - edge - tipHeight);
    }

    ::SendMessageW(tooltip_.m_hWnd, TTM_TRACKPOSITION, 0, MAKELPARAM(x, y));
    ::SendMessageW(tooltip_.m_hWnd, TTM_TRACKACTIVATE, TRUE,
                   reinterpret_cast<LPARAM>(&toolInfo_));
    tooltipVisible_ = true;
}

void NavPanel::hideTooltip()
{
    if (tooltipVisible_ && tooltip_.IsWindow()) {
        ::SendMessageW(tooltip_.m_hWnd, TTM_TRACKACTIVATE, FALSE,
                       reinterpret_cast<LPARAM>(&toolInfo_));
    }
    tooltipVisible_ = false;
}

HFONT NavPanel::tooltipFont() const
{
    return theme_ != nullptr ? theme_->uiFont()
                             : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

LRESULT NavPanel::OnSelChanged(int, LPNMHDR header, BOOL&)
{
    if (suppressSelection_) {
        return 0;
    }

    const auto* change = reinterpret_cast<LPNMTREEVIEWW>(header);
    if (change->itemNew.lParam >= 0 && callbacks_.onSelect) {
        callbacks_.onSelect(static_cast<std::size_t>(change->itemNew.lParam));
    }
    return 0;
}

} // namespace awm::ui
