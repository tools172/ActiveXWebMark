#pragma once

#include <functional>
#include <string>
#include <vector>

#include "wtlinclude.h"
#include "theme.h"

namespace awm::ui {

/// Right-hand table-of-contents panel.
///
/// The panel paints a compact header ("目录" plus a collapse chevron) and hosts
/// a themed native tree view. Heading levels are converted to a real parent /
/// child hierarchy, so sections can be collapsed independently.
class NavPanel : public CWindowImpl<NavPanel> {
public:
    DECLARE_WND_CLASS_EX(L"AwmNavPanel", CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, COLOR_WINDOW)

    static constexpr UINT kTreeId = 1;

    struct Item {
        std::wstring text;
        int level = 1;
    };

    struct Callbacks {
        std::function<void(std::size_t)> onSelect;
    };

    bool Create(HWND parent, const RECT& rc, UINT id);
    void SetTheme(const Theme* theme);
    void SetItems(std::vector<Item> items);
    void SetSelected(std::size_t index);
    int headerHeight() const;
    void SetCallbacks(Callbacks callbacks);
    void OnDpiChanged();

    HWND treeHwnd() const { return tree_.m_hWnd; }
    void FocusTree() const;

    BEGIN_MSG_MAP(NavPanel)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        NOTIFY_HANDLER(kTreeId, TVN_SELCHANGED, OnSelChanged)
    END_MSG_MAP()

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSelChanged(int, LPNMHDR, BOOL&);

    void applyTheme();
    void layoutChildren();

    CTreeViewCtrl tree_;
    const Theme* theme_ = nullptr;
    std::vector<Item> items_;
    std::vector<HTREEITEM> handles_; ///< Indexed by flat outline index.
    Callbacks callbacks_;
    bool suppressSelection_ = false;
};

} // namespace awm::ui
