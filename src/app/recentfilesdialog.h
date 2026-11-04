#pragma once

#include <string>
#include <vector>

#include "wtlinclude.h"
#include "theme.h"

namespace awm {

/// Modal "recent files" picker with paged loading.
///
/// The File menu shows the first page directly; this dialog is opened by the
/// "更多…" item and starts with one page, appending another page each time the
/// user clicks "加载更多".
class RecentFilesDialog : public CWindowImpl<RecentFilesDialog> {
public:
    DECLARE_WND_CLASS_EX(L"AwmRecentFilesDialog", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    /// Shows the dialog modally. Returns the selected path, or empty on cancel.
    static std::wstring ShowModal(HWND owner, std::vector<std::wstring> entries,
                                  std::size_t pageSize, const ui::Palette& palette);

    ~RecentFilesDialog() override;

    BEGIN_MSG_MAP(RecentFilesDialog)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
        NOTIFY_HANDLER(kListId, NM_DBLCLK, OnListDoubleClick)
        NOTIFY_HANDLER(kListId, LVN_ITEMCHANGED, OnItemChanged)
    END_MSG_MAP()

private:
    static constexpr UINT kListId = 100;
    static constexpr UINT kOpenId = 101;
    static constexpr UINT kMoreId = 102;
    static constexpr UINT kCloseId = 103;

    RecentFilesDialog(std::vector<std::wstring> entries, std::size_t pageSize,
                      ui::Palette palette);

    bool CreateModalWindow(HWND owner);
    void CreateChildren();
    void RunModal();
    void Layout();
    void AppendRows(std::size_t begin, std::size_t end);
    void UpdateFooter();
    void OpenSelected();
    void LoadMore();

    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnListDoubleClick(int, LPNMHDR, BOOL&);
    LRESULT OnItemChanged(int, LPNMHDR, BOOL&);

    ui::Theme theme_;
    ui::Palette palette_;
    HBRUSH backgroundBrush_ = nullptr;
    CListViewCtrl list_;
    CStatic info_;
    CButton openButton_;
    CButton moreButton_;
    CButton closeButton_;

    std::vector<std::wstring> entries_;
    std::size_t pageSize_ = 20;
    std::size_t displayed_ = 0;
    std::wstring selected_;
    bool done_ = false;
};

} // namespace awm
