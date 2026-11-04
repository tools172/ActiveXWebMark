#include "recentfilesdialog.h"

#include <algorithm>
#include <utility>

#include <uxtheme.h>

#include "darkmode.h"
#include "themeapply.h"
#include "util/pathutil.h"

namespace awm {

RecentFilesDialog::RecentFilesDialog(std::vector<std::wstring> entries,
                                     std::size_t pageSize, ui::Palette palette)
    : entries_(std::move(entries)),
      pageSize_(pageSize == 0 ? 20 : pageSize),
      palette_(palette)
{
}

RecentFilesDialog::~RecentFilesDialog()
{
    if (backgroundBrush_ != nullptr) {
        ::DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
}

std::wstring RecentFilesDialog::ShowModal(HWND owner, std::vector<std::wstring> entries,
                                          std::size_t pageSize,
                                          const ui::Palette& palette)
{
    RecentFilesDialog dialog(std::move(entries), pageSize, palette);
    if (!dialog.CreateModalWindow(owner)) {
        return {};
    }

    ::EnableWindow(owner, FALSE);
    dialog.ShowWindow(SW_SHOW);
    dialog.UpdateWindow();
    dialog.RunModal();
    ::EnableWindow(owner, TRUE);
    ::SetForegroundWindow(owner);
    return std::move(dialog.selected_);
}

bool RecentFilesDialog::CreateModalWindow(HWND owner)
{
    theme_.initialize(::GetDpiForWindow(owner));
    theme_.setPalette(palette_);

    const int width = theme_.px(840);
    const int height = theme_.px(470);

    RECT ownerRect = {};
    ::GetWindowRect(owner, &ownerRect);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;

    RECT rc{x, y, x + width, y + height};
    if (CWindowImpl<RecentFilesDialog>::Create(
            owner, rc, L"最近打开的文件", WS_POPUP | WS_CAPTION | WS_SYSMENU,
            WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT) == nullptr) {
        return false;
    }

    CreateChildren();
    Layout();
    return true;
}

void RecentFilesDialog::CreateChildren()
{
    RECT zero = {0, 0, 0, 0};

    backgroundBrush_ = ::CreateSolidBrush(ui::ToColorRef(theme_.palette().windowBg));

    list_.Create(m_hWnd, zero, nullptr,
                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL |
                     LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                 0, kListId);
    list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ::SetWindowTheme(list_.m_hWnd, L"Explorer", nullptr);
    ListView_SetBkColor(list_.m_hWnd, ui::ToColorRef(theme_.palette().surface));
    ListView_SetTextBkColor(list_.m_hWnd, ui::ToColorRef(theme_.palette().surface));
    ListView_SetTextColor(list_.m_hWnd, ui::ToColorRef(theme_.palette().textPrimary));
    ::SendMessageW(list_.m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(theme_.uiFont()), TRUE);

    const ColorMode mode =
        IsDarkPalette(theme_.palette()) ? ColorMode::Dark : ColorMode::Light;
    ApplyColorModeToWindow(m_hWnd, mode);
    ApplyColorModeToWindow(list_.m_hWnd, mode);
    ui::SetThemedScrollbar(list_.m_hWnd, MakeScrollbarColors(theme_.palette()));
    list_.InsertColumn(0, L"序号", LVCFMT_RIGHT, theme_.px(64));
    list_.InsertColumn(1, L"文件", LVCFMT_LEFT, theme_.px(280));
    list_.InsertColumn(2, L"位置", LVCFMT_LEFT, theme_.px(380));

    info_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX);
    ::SendMessageW(info_.m_hWnd, WM_SETFONT,
                   reinterpret_cast<WPARAM>(theme_.smallFont()), TRUE);

    openButton_.Create(m_hWnd, zero, L"打开(&O)",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, kOpenId);
    moreButton_.Create(m_hWnd, zero, L"加载更多(&M)",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, kMoreId);
    closeButton_.Create(m_hWnd, zero, L"关闭(&C)",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, kCloseId);

    for (CButton* button : {&openButton_, &moreButton_, &closeButton_}) {
        ::SendMessageW(button->m_hWnd, WM_SETFONT,
                       reinterpret_cast<WPARAM>(theme_.uiFont()), TRUE);
    }

    const std::size_t initial = std::min(pageSize_, entries_.size());
    AppendRows(0, initial);
    displayed_ = initial;
    UpdateFooter();

    if (list_.GetItemCount() > 0) {
        list_.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        list_.SetFocus();
    }
}

void RecentFilesDialog::Layout()
{
    if (!IsWindow() || !list_.IsWindow()) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);

    const int margin = theme_.px(16);
    const int gap = theme_.px(8);
    const int buttonHeight = theme_.px(30);
    const int infoHeight = theme_.px(20);
    const int bottom = client.bottom - margin - buttonHeight;

    RECT listRect{margin, margin, client.right - margin, bottom - theme_.px(10)};
    if (listRect.bottom < listRect.top) {
        listRect.bottom = listRect.top;
    }
    list_.MoveWindow(&listRect, TRUE);

    const int closeWidth = theme_.px(90);
    const int moreWidth = theme_.px(112);
    const int openWidth = theme_.px(90);

    int buttonX = client.right - margin - closeWidth;
    closeButton_.MoveWindow(buttonX, bottom, closeWidth, buttonHeight, TRUE);
    buttonX -= gap + moreWidth;
    moreButton_.MoveWindow(buttonX, bottom, moreWidth, buttonHeight, TRUE);
    buttonX -= gap + openWidth;
    openButton_.MoveWindow(buttonX, bottom, openWidth, buttonHeight, TRUE);

    RECT infoRect{margin, bottom + (buttonHeight - infoHeight) / 2, buttonX - gap,
                  bottom + (buttonHeight + infoHeight) / 2};
    if (infoRect.right < infoRect.left) {
        infoRect.right = infoRect.left;
    }
    info_.MoveWindow(&infoRect, TRUE);

    const int listWidth = std::max(0, static_cast<int>(listRect.right - listRect.left));
    const int numberWidth = theme_.px(64);
    const int remaining = std::max(0, listWidth - numberWidth - theme_.px(4));
    const int nameWidth = static_cast<int>(remaining * 0.44);
    list_.SetColumnWidth(0, numberWidth);
    list_.SetColumnWidth(1, nameWidth);
    list_.SetColumnWidth(2, std::max(0, remaining - nameWidth));
}

void RecentFilesDialog::AppendRows(std::size_t begin, std::size_t end)
{
    end = std::min(end, entries_.size());
    for (std::size_t i = begin; i < end; ++i) {
        const std::wstring number = std::to_wstring(i + 1);
        const std::wstring name = fileName(entries_[i]);
        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = list_.GetItemCount();
        item.pszText = const_cast<LPWSTR>(number.c_str());
        item.lParam = static_cast<LPARAM>(i);

        const int index = list_.InsertItem(&item);
        if (index >= 0) {
            list_.SetItemText(index, 1, name.c_str());
            const std::wstring directory = fileDirectory(entries_[i]);
            list_.SetItemText(index, 2, directory.c_str());
        }
    }
    if (list_.IsWindow()) {
        ::RedrawWindow(list_.m_hWnd, nullptr, nullptr,
                       RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
    }
}

void RecentFilesDialog::UpdateFooter()
{
    const std::wstring text = L"显示 " + std::to_wstring(displayed_) + L" / " +
                              std::to_wstring(entries_.size()) + L" 项";
    info_.SetWindowText(text.c_str());
    moreButton_.EnableWindow(displayed_ < entries_.size() ? TRUE : FALSE);
    openButton_.EnableWindow(list_.GetSelectedCount() > 0 ? TRUE : FALSE);
}

void RecentFilesDialog::OpenSelected()
{
    const int selected = list_.GetNextItem(-1, LVNI_SELECTED);
    if (selected < 0) {
        return;
    }
    const auto index = static_cast<std::size_t>(list_.GetItemData(selected));
    if (index >= entries_.size()) {
        return;
    }

    selected_ = entries_[index];
    done_ = true;
    DestroyWindow();
}

void RecentFilesDialog::LoadMore()
{
    if (displayed_ >= entries_.size()) {
        return;
    }
    const std::size_t begin = displayed_;
    const std::size_t end = std::min(entries_.size(), displayed_ + pageSize_);
    AppendRows(begin, end);
    displayed_ = end;
    UpdateFooter();
}

void RecentFilesDialog::RunModal()
{
    MSG message = {};
    while (!done_) {
        const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) {
                ::PostQuitMessage(static_cast<int>(message.wParam));
            }
            break;
        }

        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            done_ = true;
            DestroyWindow();
            continue;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN) {
            const HWND focus = ::GetFocus();
            if (focus == list_.m_hWnd || focus == info_.m_hWnd) {
                OpenSelected();
                continue;
            }
        }

        if (::IsDialogMessageW(m_hWnd, &message) == FALSE) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }
}

LRESULT RecentFilesDialog::OnCommand(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    switch (LOWORD(wParam)) {
    case kOpenId:
    case IDOK:
        OpenSelected();
        break;
    case kMoreId:
        LoadMore();
        break;
    case kCloseId:
    case IDCANCEL:
        done_ = true;
        DestroyWindow();
        break;
    default:
        break;
    }
    return 0;
}

LRESULT RecentFilesDialog::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);
    HBRUSH brush = ::CreateSolidBrush(ui::ToColorRef(theme_.palette().windowBg));
    if (brush != nullptr) {
        ::FillRect(dc, &client, brush);
        ::DeleteObject(brush);
    }

    EndPaint(&ps);
    return 0;
}

LRESULT RecentFilesDialog::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT RecentFilesDialog::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    Layout();
    return 0;
}

LRESULT RecentFilesDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    done_ = true;
    DestroyWindow();
    return 0;
}

LRESULT RecentFilesDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    done_ = true;
    return 0;
}

LRESULT RecentFilesDialog::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    HDC dc = reinterpret_cast<HDC>(wParam);
    const HWND control = reinterpret_cast<HWND>(lParam);
    const ui::Palette& palette = theme_.palette();
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ui::ToColorRef(control == info_.m_hWnd ? palette.textSecondary
                                                               : palette.textPrimary));
    return reinterpret_cast<LRESULT>(backgroundBrush_);
}

LRESULT RecentFilesDialog::OnListDoubleClick(int, LPNMHDR, BOOL&)
{
    OpenSelected();
    return 0;
}

LRESULT RecentFilesDialog::OnItemChanged(int, LPNMHDR, BOOL&)
{
    UpdateFooter();
    return 0;
}

} // namespace awm
