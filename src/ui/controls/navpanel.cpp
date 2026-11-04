#include "controls/navpanel.h"

#include <algorithm>
#include <utility>

#include <uxtheme.h>

#include "layout/navrows.h"

namespace awm::ui {

namespace {

void fillSolid(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = ::CreateSolidBrush(color);
    if (brush != nullptr) {
        ::FillRect(dc, &rc, brush);
        ::DeleteObject(brush);
    }
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

    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS |
                        TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_TRACKSELECT |
                        TVS_NOTOOLTIPS | TVS_DISABLEDRAGDROP;
    RECT treeRect = {0, 0, 0, 0};
    if (tree_.Create(m_hWnd, treeRect, nullptr, style, 0, kTreeId) == nullptr) {
        return false;
    }

    ::SetWindowTheme(tree_.m_hWnd, L"Explorer", nullptr);
    TreeView_SetExtendedStyle(tree_.m_hWnd, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    layoutChildren();
    applyTheme();
    return true;
}

void NavPanel::SetTheme(const Theme* theme)
{
    theme_ = theme;
    applyTheme();
    layoutChildren();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void NavPanel::SetItems(std::vector<Item> items)
{
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
    TreeView_SetBkColor(tree_.m_hWnd, ToColorRef(palette.surface));
    TreeView_SetTextColor(tree_.m_hWnd, ToColorRef(palette.textPrimary));
    TreeView_SetLineColor(tree_.m_hWnd, ToColorRef(palette.divider));
    TreeView_SetItemHeight(tree_.m_hWnd, theme_->px(24));
    TreeView_SetIndent(tree_.m_hWnd, theme_->px(14));
}

void NavPanel::layoutChildren()
{
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

    fillSolid(dc, client, ToColorRef(palette.navBg));

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
