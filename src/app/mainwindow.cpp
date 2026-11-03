#include "mainwindow.h"

#include <shellapi.h>

#include "apppaths.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <utility>

#include "dialogs.h"
#include "doublebuffer.h"
#include "filedialogs.h"
#include "recentfilesdialog.h"
#include "resource.h"
#include "scintillaeditor.h"
#include "util/pathutil.h"
#include "util/stringutil.h"
#include "version.h"

namespace awm {

namespace {

std::wstring escapeMenuText(std::wstring text)
{
    std::wstring escaped;
    escaped.reserve(text.size() + 4);
    for (const wchar_t ch : text) {
        if (ch == L'&') {
            escaped += L"&&";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::wstring menuLabelForRecent(const std::wstring& path, std::size_t index)
{
    std::wstring label = std::to_wstring(index + 1) + L". " + escapeMenuText(fileName(path));
    const std::wstring directory = fileDirectory(path);
    if (!directory.empty()) {
        label += L"  —  " + escapeMenuText(directory);
    }
    if (label.size() > 88) {
        label = label.substr(0, 87) + L"…";
    }
    return label;
}

void copyTextToClipboard(HWND owner, const std::wstring& text)
{
    if (text.empty() || !::OpenClipboard(owner)) {
        return;
    }
    ::EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr) {
        void* target = ::GlobalLock(memory);
        if (target != nullptr) {
            std::memcpy(target, text.c_str(), bytes);
            ::GlobalUnlock(memory);
        }
        if (::SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
            ::GlobalFree(memory);
        }
    }
    ::CloseClipboard();
}

void revealInExplorer(HWND owner, const std::wstring& path)
{
    if (path.empty()) {
        return;
    }

    std::wstring arguments = L"/select,\"" + path + L"\"";
    const INT_PTR result = reinterpret_cast<INT_PTR>(
        ::ShellExecuteW(owner, L"open", L"explorer.exe", arguments.c_str(), nullptr,
                        SW_SHOWNORMAL));
    if (result > 32) {
        return;
    }

    // The file may have been moved or deleted; fall back to its folder.
    const std::wstring directory = fileDirectory(path);
    if (!directory.empty()) {
        ::ShellExecuteW(owner, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

} // namespace

MainWindow::MainWindow() : registry_(DocumentFormatRegistry::createDefault())
{
    theme_.initialize(96);
    settings_ = createSettings();
    if (settings_ != nullptr) {
        recent_.load(*settings_, "history.recentFiles");
        viewSettings_ = LoadViewSettings(*settings_);
        placement_ = LoadWindowPlacement(*settings_);
    }
    startMaximized_ = placement_.valid && placement_.maximized;
}

MainWindow::~MainWindow()
{
    if (mainMenu_ != nullptr) {
        ::DestroyMenu(mainMenu_);
        mainMenu_ = nullptr;
        fileMenu_ = nullptr;
        viewMenu_ = nullptr;
        recentMenu_ = nullptr;
    }
}

BOOL MainWindow::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F1) {
        SendCommand(cmd::kHelpAbout);
        return TRUE;
    }

    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN) {
        const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0;

        if (ctrl && alt && !shift && pMsg->wParam == L'L') {
            SendCommand(cmd::kViewToggleSplit);
            return TRUE;
        }
        if (ctrl && !alt) {
            switch (pMsg->wParam) {
            case L'O':
                SendCommand(cmd::kFileOpen);
                return TRUE;
            case L'S':
                SendCommand(shift ? cmd::kFileSaveAs : cmd::kFileSave);
                return TRUE;
            case L'W':
                SendCommand(cmd::kFileCloseTab);
                return TRUE;
            case L'E':
                SendCommand(cmd::kViewEditor);
                return TRUE;
            case L'B':
                SendCommand(cmd::kViewNav);
                return TRUE;
            case L'L':
                if (shift) {
                    SendCommand(cmd::kViewToggleSplit);
                    return TRUE;
                }
                break;
            case VK_TAB:
                CycleTab(shift ? -1 : 1);
                return TRUE;
            default:
                break;
            }
        }
    }

    // Let the preview handle its own accelerators (copy, select all, ...).
    if (DocumentView* view = ActiveView()) {
        if (view->ForwardAccelerator(pMsg)) {
            return TRUE;
        }
    }
    return FALSE;
}

void MainWindow::SendCommand(int id)
{
    ::SendMessageW(m_hWnd, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(id), 1), 0);
}

DocumentView* MainWindow::ActiveView() const
{
    if (activeIndex_ < 0 || activeIndex_ >= static_cast<int>(views_.size())) {
        return nullptr;
    }
    return views_[static_cast<std::size_t>(activeIndex_)].get();
}

DocumentViewServices MainWindow::MakeServices()
{
    DocumentViewServices services;
    services.engine = &engine_;
    services.highlighter = &highlighter_;
    services.theme = &theme_;
    services.createEditor = []() -> std::unique_ptr<IEditorView> {
        return std::make_unique<ScintillaEditor>();
    };
    services.createPreview = []() -> std::unique_ptr<IPreviewView> {
        return CreatePreviewView(PreviewEngine::Ie);
    };
    services.onOpenLink = [](const std::string& url) {
        if (!url.empty()) {
            ::ShellExecuteW(nullptr, L"open", utf8ToWide(url).c_str(), nullptr, nullptr,
                            SW_SHOWNORMAL);
        }
    };
    return services;
}

LRESULT MainWindow::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
    const UINT dpi = ::GetDpiForWindow(m_hWnd);
    theme_.initialize(dpi);

    // Title bar / taskbar icon from the executable's embedded resource.
    const HINSTANCE instance = ::GetModuleHandleW(nullptr);
    const auto loadAppIcon = [instance, dpi](int metricX, int metricY) -> HICON {
        return static_cast<HICON>(::LoadImageW(
            instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
            ::GetSystemMetricsForDpi(metricX, dpi),
            ::GetSystemMetricsForDpi(metricY, dpi),
            LR_DEFAULTCOLOR | LR_SHARED));
    };
    if (HICON bigIcon = loadAppIcon(SM_CXICON, SM_CYICON)) {
        SetIcon(bigIcon, TRUE);
    }
    if (HICON smallIcon = loadAppIcon(SM_CXSMICON, SM_CYSMICON)) {
        SetIcon(smallIcon, FALSE);
    }

    if (placement_.valid) {
        ::SetWindowPos(m_hWnd, nullptr, placement_.x, placement_.y, placement_.width,
                       placement_.height, SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
        RECT work = {};
        ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int width = std::min(theme_.px(1180), static_cast<int>(work.right - work.left));
        const int height = std::min(theme_.px(780), static_cast<int>(work.bottom - work.top));
        const int x = work.left + (static_cast<int>(work.right - work.left) - width) / 2;
        const int y = work.top + (static_cast<int>(work.bottom - work.top) - height) / 2;
        ::SetWindowPos(m_hWnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    RECT zero = {0, 0, 0, 0};
    tabStrip_.Create(m_hWnd, zero, 2000);
    tabStrip_.SetTheme(&theme_);
    tabStrip_.SetCallbacks(ui::TabStrip::Callbacks{
        [this](int index) { ActivateTab(index); },
        [this](int index) { CloseTab(index); },
        [this]() { SendCommand(cmd::kFileOpen); },
        [this]() { SendCommand(cmd::kViewNav); },
        [this](int index, POINT screen) { ShowTabContextMenu(index, screen); }});

    CreateMenuBar();
    RebuildRecentMenu();
    UpdateMenuState();
    Layout();
    RestoreSession();
    return 0;
}

void MainWindow::CreateMenuBar()
{
    if (mainMenu_ != nullptr) {
        return;
    }

    mainMenu_ = ::CreateMenu();
    fileMenu_ = ::CreatePopupMenu();
    viewMenu_ = ::CreatePopupMenu();
    recentMenu_ = ::CreatePopupMenu();

    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileOpen, L"打开(&O)...\tCtrl+O");
    ::AppendMenuW(fileMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(recentMenu_),
                  L"最近打开(&R)");
    ::AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileSave, L"保存(&S)\tCtrl+S");
    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileSaveAs, L"另存为(&A)...\tCtrl+Shift+S");
    ::AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileCloseTab, L"关闭标签(&W)\tCtrl+W");
    ::AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileExit, L"退出(&X)");

    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewEditor, L"显示编辑器(&E)\tCtrl+E");
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewNav, L"显示导航目录(&B)\tCtrl+B");
    ::AppendMenuW(viewMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewSplitSideBySide, L"左右分屏(&L)");
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewSplitStacked, L"上下分屏(&T)");
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewToggleSplit,
                  L"切换分屏方向(&S)\tCtrl+Shift+L");
    ::AppendMenuW(viewMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewScrollSync, L"滚动同步(&Y)");

    HMENU helpMenu = ::CreatePopupMenu();
    ::AppendMenuW(helpMenu, MF_STRING, cmd::kHelpAbout, L"关于 ActiveXWebMark(&A)...\tF1");

    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu_), L"文件(&F)");
    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu_), L"视图(&V)");
    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"帮助(&H)");
    ::SetMenu(m_hWnd, mainMenu_);
}

void MainWindow::RebuildRecentMenu()
{
    if (recentMenu_ == nullptr) {
        return;
    }

    while (::GetMenuItemCount(recentMenu_) > 0) {
        ::DeleteMenu(recentMenu_, 0, MF_BYPOSITION);
    }

    const std::vector<std::wstring> page = recent_.menuPage();
    if (page.empty()) {
        ::AppendMenuW(recentMenu_, MF_STRING | MF_GRAYED, 0, L"（暂无记录）");
    } else {
        for (std::size_t i = 0; i < page.size(); ++i) {
            const std::wstring label = menuLabelForRecent(page[i], i);
            ::AppendMenuW(recentMenu_, MF_STRING, cmd::kRecentBase + static_cast<int>(i),
                          label.c_str());
        }
        if (recent_.size() > page.size()) {
            ::AppendMenuW(recentMenu_, MF_SEPARATOR, 0, nullptr);
            ::AppendMenuW(recentMenu_, MF_STRING, cmd::kRecentMore, L"更多(&M)...");
        }
    }

    if (::IsWindow(m_hWnd)) {
        ::DrawMenuBar(m_hWnd);
    }
}

void MainWindow::UpdateMenuState()
{
    if (viewMenu_ == nullptr) {
        return;
    }

    ::CheckMenuItem(viewMenu_, cmd::kViewEditor,
                    MF_BYCOMMAND |
                        (viewSettings_.showEditor ? MF_CHECKED : MF_UNCHECKED));
    ::CheckMenuItem(viewMenu_, cmd::kViewNav,
                    MF_BYCOMMAND | (viewSettings_.showNav ? MF_CHECKED : MF_UNCHECKED));
    if (tabStrip_.IsWindow()) {
        tabStrip_.SetNavVisible(viewSettings_.showNav);
    }
    ::CheckMenuItem(viewMenu_, cmd::kViewScrollSync,
                    MF_BYCOMMAND | (viewSettings_.scrollSync ? MF_CHECKED : MF_UNCHECKED));
    ::CheckMenuRadioItem(
        viewMenu_, cmd::kViewSplitSideBySide, cmd::kViewSplitStacked,
        viewSettings_.split == ui::Orientation::Horizontal ? cmd::kViewSplitSideBySide
                                                           : cmd::kViewSplitStacked,
        MF_BYCOMMAND);

    const bool hasView = ActiveView() != nullptr;
    const UINT enabled = hasView ? MF_ENABLED : MF_GRAYED;
    ::EnableMenuItem(fileMenu_, cmd::kFileSave, MF_BYCOMMAND | enabled);
    ::EnableMenuItem(fileMenu_, cmd::kFileSaveAs, MF_BYCOMMAND | enabled);
    ::EnableMenuItem(fileMenu_, cmd::kFileCloseTab, MF_BYCOMMAND | enabled);
}

void MainWindow::UpdateTabStrip()
{
    if (!tabStrip_.IsWindow()) {
        return;
    }

    std::vector<ui::TabStrip::Item> items;
    items.reserve(views_.size());
    for (const std::unique_ptr<DocumentView>& view : views_) {
        items.push_back(ui::TabStrip::Item{view->session().title(), view->session().path(),
                                           view->session().isDirty()});
    }
    tabStrip_.SetItems(std::move(items), activeIndex_);
}

void MainWindow::UpdateWindowTitle()
{
    std::wstring title = L"ActiveXWebMark";
    if (DocumentView* view = ActiveView()) {
        title = view->session().title();
        if (view->session().isDirty()) {
            title = L"*" + title;
        }
        title += L" - ActiveXWebMark";
    }
    SetWindowText(title.c_str());
}

void MainWindow::Layout()
{
    if (!::IsWindow(m_hWnd) || !tabStrip_.IsWindow()) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);

    if (views_.empty()) {
        tabStrip_.ShowWindow(SW_HIDE);
        Invalidate(FALSE);
        return;
    }

    tabStrip_.ShowWindow(SW_SHOW);
    const int stripHeight = tabStrip_.preferredHeight();
    tabStrip_.MoveWindow(0, 0, static_cast<int>(client.right), stripHeight, TRUE);

    RECT content{0, stripHeight, client.right, client.bottom};
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (static_cast<int>(i) == activeIndex_) {
            views_[i]->ShowWindow(SW_SHOW);
            views_[i]->SetBounds(content);
        } else {
            views_[i]->ShowWindow(SW_HIDE);
        }
    }
    Invalidate(FALSE);
}

int MainWindow::FindTabByPath(const std::wstring& path) const
{
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (samePath(views_[i]->session().path(), path)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int MainWindow::OpenFileInternal(const std::wstring& path, bool activate, bool reportErrors)
{
    if (path.empty()) {
        return -1;
    }

    const std::wstring normalized = normalizePath(path);
    const int existing = FindTabByPath(normalized);
    if (existing >= 0) {
        if (settings_ != nullptr) {
            recent_.add(normalized);
            recent_.save(*settings_, "history.recentFiles");
            RebuildRecentMenu();
        }
        if (activate) {
            ActivateTab(existing);
        }
        return existing;
    }

    std::error_code ec;
    std::unique_ptr<IDocument> document = registry_.load(normalized, ec);
    if (!document) {
        if (reportErrors) {
            std::wstring message = L"无法打开文件：\n" + normalized;
            if (ec) {
                message += L"\n\n" + utf8ToWide(ec.message());
            }
            ShowError(m_hWnd, message);
        }
        if (settings_ != nullptr && !std::filesystem::exists(normalized)) {
            recent_.remove(normalized);
            recent_.save(*settings_, "history.recentFiles");
            RebuildRecentMenu();
        }
        return -1;
    }

    auto session = DocumentSession::fromDocument(normalized, *document);
    auto view = std::make_unique<DocumentView>();
    view->SetCallbacks(DocumentViewCallbacks{
        [this](DocumentView&) {
            UpdateTabStrip();
            UpdateWindowTitle();
        },
        [this](const ViewSettings& settings) {
            viewSettings_ = settings;
            ApplyViewSettingsToAll();
            UpdateMenuState();
        }});

    RECT zero = {0, 0, 0, 0};
    if (!view->Create(m_hWnd, zero, std::move(session), MakeServices())) {
        if (reportErrors) {
            ShowError(m_hWnd, L"无法创建文档视图。");
        }
        return -1;
    }
    view->SetViewSettings(viewSettings_);
    views_.push_back(std::move(view));

    const int index = static_cast<int>(views_.size()) - 1;
    if (settings_ != nullptr) {
        recent_.add(normalized);
        recent_.save(*settings_, "history.recentFiles");
        RebuildRecentMenu();
    }
    UpdateTabStrip();

    if (activate) {
        ActivateTab(index);
    }
    return index;
}

void MainWindow::OpenFile(const std::wstring& path)
{
    OpenFileInternal(path, true);
}

void MainWindow::OpenFiles(const std::vector<std::wstring>& paths)
{
    if (paths.empty()) {
        return;
    }

    int firstOpened = -1;
    int firstNew = -1;
    for (const std::wstring& path : paths) {
        // Indices at or beyond the current size are tabs created by this call.
        const std::size_t before = views_.size();
        const int index = OpenFileInternal(path, false);
        if (index < 0) {
            continue;
        }
        if (firstOpened < 0) {
            firstOpened = index;
        }
        if (static_cast<std::size_t>(index) >= before && firstNew < 0) {
            firstNew = index;
        }
    }

    const int target = firstNew >= 0 ? firstNew : firstOpened;
    if (target >= 0) {
        ActivateTab(target);
    } else if (activeIndex_ < 0 && !views_.empty()) {
        ActivateTab(0);
    }
}

void MainWindow::RestoreSession()
{
    if (settings_ == nullptr) {
        return;
    }

    const SessionState session = LoadSessionState(*settings_);
    int restored = 0;
    int active = -1;
    for (const std::wstring& path : session.files) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            continue; // The file was moved or deleted since the last run.
        }

        const int index = OpenFileInternal(path, false, false);
        if (index < 0) {
            continue;
        }
        ++restored;
        if (active < 0 && !session.activeFile.empty() &&
            samePath(path, session.activeFile)) {
            active = index;
        }
    }

    if (active >= 0) {
        ActivateTab(active);
    } else if (restored > 0) {
        ActivateTab(0);
    }
}

void MainWindow::SaveSession()
{
    if (settings_ == nullptr) {
        return;
    }

    std::vector<std::wstring> files;
    files.reserve(views_.size());
    for (const std::unique_ptr<DocumentView>& view : views_) {
        if (view->session().hasPath()) {
            files.push_back(view->session().path());
        }
    }

    std::wstring active;
    if (activeIndex_ >= 0 && activeIndex_ < static_cast<int>(views_.size())) {
        active = views_[activeIndex_]->session().path();
    }
    SaveSessionState(*settings_, files, active);
}

void MainWindow::ActivateTab(int index)
{
    if (index < 0 || index >= static_cast<int>(views_.size())) {
        return;
    }

    if (index == activeIndex_) {
        Layout();
        if (DocumentView* view = ActiveView()) {
            view->FocusPrimaryPane();
        }
        return;
    }

    if (DocumentView* previous = ActiveView()) {
        previous->Deactivate();
    }

    activeIndex_ = index;
    DocumentView* view = ActiveView();
    view->SetViewSettings(viewSettings_);

    RECT client = {};
    GetClientRect(&client);
    const int stripHeight = tabStrip_.preferredHeight();
    view->SetBounds(RECT{0, stripHeight, client.right, client.bottom});
    view->Activate();

    tabStrip_.SetActiveIndex(index);
    Layout();
    UpdateWindowTitle();
    UpdateMenuState();
}

void MainWindow::CycleTab(int delta)
{
    if (views_.size() < 2 || activeIndex_ < 0) {
        return;
    }
    const int count = static_cast<int>(views_.size());
    int next = (activeIndex_ + delta) % count;
    if (next < 0) {
        next += count;
    }
    ActivateTab(next);
}

void MainWindow::CloseTab(int index)
{
    if (index < 0 || index >= static_cast<int>(views_.size())) {
        return;
    }

    DocumentView* view = views_[static_cast<std::size_t>(index)].get();
    if (!ConfirmClose(*view)) {
        return;
    }

    const bool wasActive = index == activeIndex_;
    if (wasActive) {
        view->Deactivate();
    }
    views_.erase(views_.begin() + index);

    if (views_.empty()) {
        activeIndex_ = -1;
        UpdateTabStrip();
        UpdateWindowTitle();
        UpdateMenuState();
        Layout();
        Invalidate(FALSE);
        return;
    }

    if (wasActive) {
        activeIndex_ = -1;
        UpdateTabStrip();
        ActivateTab(std::min(index, static_cast<int>(views_.size()) - 1));
    } else {
        if (index < activeIndex_) {
            --activeIndex_;
        }
        UpdateTabStrip();
        UpdateWindowTitle();
        UpdateMenuState();
        Layout();
    }
}

void MainWindow::CloseTabsAt(std::vector<int> indices, int preferredIndex)
{
    indices.erase(std::remove_if(indices.begin(), indices.end(),
                                 [this](int index) {
                                     return index < 0 ||
                                            index >= static_cast<int>(views_.size());
                                 }),
                  indices.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (indices.empty()) {
        return;
    }

    // Confirm every dirty document before touching the tab list, so a Cancel
    // leaves all tabs in place instead of closing only part of the group.
    for (const int index : indices) {
        if (!ConfirmClose(*views_[static_cast<std::size_t>(index)])) {
            return;
        }
    }

    DocumentView* preferredView =
        (preferredIndex >= 0 && preferredIndex < static_cast<int>(views_.size()))
            ? views_[static_cast<std::size_t>(preferredIndex)].get()
            : nullptr;
    DocumentView* activeView = ActiveView();

    const bool activeClosed =
        activeIndex_ < 0 ||
        std::binary_search(indices.begin(), indices.end(), activeIndex_);
    int removedBeforeActive = 0;
    for (const int index : indices) {
        if (index < activeIndex_) {
            ++removedBeforeActive;
        }
    }

    if (activeClosed && activeView != nullptr) {
        activeView->Deactivate();
    }

    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        views_.erase(views_.begin() + *it);
    }

    if (views_.empty()) {
        activeIndex_ = -1;
        UpdateTabStrip();
        UpdateWindowTitle();
        UpdateMenuState();
        Layout();
        Invalidate(FALSE);
        return;
    }

    if (!activeClosed) {
        activeIndex_ -= removedBeforeActive;
        UpdateTabStrip();
        UpdateWindowTitle();
        UpdateMenuState();
        Layout();
        return;
    }

    int target = -1;
    for (int i = 0; i < static_cast<int>(views_.size()); ++i) {
        if (views_[static_cast<std::size_t>(i)].get() == preferredView) {
            target = i;
            break;
        }
    }
    if (target < 0) {
        target = std::clamp(activeIndex_ - removedBeforeActive, 0,
                            static_cast<int>(views_.size()) - 1);
    }
    activeIndex_ = -1;
    UpdateTabStrip();
    ActivateTab(target);
}

void MainWindow::ShowTabContextMenu(int index, POINT screen)
{
    if (index < 0 || index >= static_cast<int>(views_.size())) {
        return;
    }

    DocumentView& view = *views_[static_cast<std::size_t>(index)];
    const std::wstring& path = view.session().path();
    const bool hasPath = view.session().hasPath();
    const int count = static_cast<int>(views_.size());

    enum MenuCommand : UINT {
        kClose = 1,
        kCloseOthers,
        kCloseLeft,
        kCloseRight,
        kReveal,
        kCopyPath,
        kCopyName,
    };

    HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const UINT pathFlags = MF_STRING | (hasPath ? MF_ENABLED : MF_GRAYED);
    ::AppendMenuW(menu, MF_STRING, kClose, L"关闭(&C)\tCtrl+W");
    ::AppendMenuW(menu, MF_STRING | (count > 1 ? MF_ENABLED : MF_GRAYED), kCloseOthers,
                  L"关闭其他标签(&O)");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING | (index > 0 ? MF_ENABLED : MF_GRAYED), kCloseLeft,
                  L"关闭左侧标签(&L)");
    ::AppendMenuW(menu, MF_STRING | (index + 1 < count ? MF_ENABLED : MF_GRAYED),
                  kCloseRight, L"关闭右侧标签(&R)");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, pathFlags, kReveal, L"在资源管理器中显示(&E)");
    ::AppendMenuW(menu, pathFlags, kCopyPath, L"复制文件路径(&P)");
    ::AppendMenuW(menu, pathFlags, kCopyName, L"复制文件名(&N)");

    ::SetForegroundWindow(m_hWnd);
    const UINT command = ::TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, m_hWnd, nullptr);
    ::DestroyMenu(menu);
    ::PostMessageW(m_hWnd, WM_NULL, 0, 0);

    switch (command) {
    case kClose:
        CloseTab(index);
        break;
    case kCloseOthers: {
        std::vector<int> indices;
        for (int i = 0; i < count; ++i) {
            if (i != index) {
                indices.push_back(i);
            }
        }
        CloseTabsAt(std::move(indices), index);
        break;
    }
    case kCloseLeft: {
        std::vector<int> indices;
        for (int i = 0; i < index; ++i) {
            indices.push_back(i);
        }
        CloseTabsAt(std::move(indices), index);
        break;
    }
    case kCloseRight: {
        std::vector<int> indices;
        for (int i = index + 1; i < count; ++i) {
            indices.push_back(i);
        }
        CloseTabsAt(std::move(indices), index);
        break;
    }
    case kReveal:
        revealInExplorer(m_hWnd, path);
        break;
    case kCopyPath:
        copyTextToClipboard(m_hWnd, path);
        break;
    case kCopyName:
        copyTextToClipboard(m_hWnd, fileName(path));
        break;
    default:
        break;
    }
}

bool MainWindow::ConfirmClose(DocumentView& view)
{
    if (!view.session().isDirty()) {
        return true;
    }

    switch (AskSaveChanges(m_hWnd, view.session().title())) {
    case SaveDecision::Save:
        return SaveView(view);
    case SaveDecision::Discard:
        return true;
    case SaveDecision::Cancel:
    default:
        return false;
    }
}

bool MainWindow::ConfirmCloseAll()
{
    for (const std::unique_ptr<DocumentView>& view : views_) {
        if (!ConfirmClose(*view)) {
            return false;
        }
    }
    return true;
}

bool MainWindow::SaveView(DocumentView& view)
{
    if (!view.session().hasPath()) {
        return SaveViewAs(view);
    }

    std::error_code ec;
    if (!view.Save(ec)) {
        ShowError(m_hWnd, L"保存失败：\n" + view.session().path() + L"\n\n" +
                              utf8ToWide(ec.message()));
        return false;
    }

    if (settings_ != nullptr) {
        recent_.add(view.session().path());
        recent_.save(*settings_, "history.recentFiles");
        RebuildRecentMenu();
    }
    UpdateTabStrip();
    UpdateWindowTitle();
    return true;
}

bool MainWindow::SaveViewAs(DocumentView& view)
{
    const std::wstring suggested =
        view.session().hasPath() ? view.session().path() : view.session().title() + L".md";
    const std::wstring path = ShowSaveMarkdownDialog(m_hWnd, suggested);
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    if (!view.SaveAs(path, ec)) {
        ShowError(m_hWnd, L"保存失败：\n" + path + L"\n\n" + utf8ToWide(ec.message()));
        return false;
    }

    if (settings_ != nullptr) {
        recent_.add(view.session().path());
        recent_.save(*settings_, "history.recentFiles");
        RebuildRecentMenu();
    }
    UpdateTabStrip();
    UpdateWindowTitle();
    return true;
}

bool MainWindow::SaveActive()
{
    DocumentView* view = ActiveView();
    return view != nullptr && SaveView(*view);
}

bool MainWindow::SaveActiveAs()
{
    DocumentView* view = ActiveView();
    return view != nullptr && SaveViewAs(*view);
}

void MainWindow::ApplyViewSettingsToAll()
{
    for (const std::unique_ptr<DocumentView>& view : views_) {
        view->SetViewSettings(viewSettings_);
    }
    Layout();
}

void MainWindow::SaveSettings()
{
    if (settings_ == nullptr) {
        return;
    }
    SaveViewSettings(*settings_, viewSettings_);
    recent_.save(*settings_, "history.recentFiles");
    SaveWindowPlacement(*settings_, m_hWnd);
}

LRESULT MainWindow::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    // OnClose normally saves first; this covers shutdown paths that bypass it.
    if (!sessionSaved_) {
        SaveSession();
        sessionSaved_ = true;
    }
    SaveSettings();
    views_.clear();
    activeIndex_ = -1;

    if (mainMenu_ != nullptr) {
        ::DestroyMenu(mainMenu_);
        mainMenu_ = nullptr;
        fileMenu_ = nullptr;
        viewMenu_ = nullptr;
        recentMenu_ = nullptr;
    }

    ::PostQuitMessage(0);
    return 0;
}

LRESULT MainWindow::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    if (!ConfirmCloseAll()) {
        return 0;
    }

    SaveSession();
    sessionSaved_ = true;
    views_.clear();
    activeIndex_ = -1;
    DestroyWindow();
    return 0;
}

LRESULT MainWindow::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    Layout();
    return 0;
}

LRESULT MainWindow::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);
    if (client.right <= 0 || client.bottom <= 0) {
        EndPaint(&ps);
        return 0;
    }

    ui::DoubleBuffer buffer(dc, static_cast<int>(client.right),
                            static_cast<int>(client.bottom));
    HDC target = buffer.valid() ? buffer.dc() : dc;

    const ui::Palette& palette = theme_.palette();
    HBRUSH brush = ::CreateSolidBrush(ui::ToColorRef(palette.windowBg));
    if (brush != nullptr) {
        ::FillRect(target, &client, brush);
        ::DeleteObject(brush);
    }

    if (views_.empty()) {
        PaintWelcome(target, client);
    }

    if (buffer.valid()) {
        buffer.present();
    }

    EndPaint(&ps);
    return 0;
}

void MainWindow::PaintWelcome(HDC dc, const RECT& client)
{
    const ui::Palette& palette = theme_.palette();
    const int center = client.top + (client.bottom - client.top) / 2;

    RECT titleRect{client.left, center - theme_.px(84), client.right, center - theme_.px(36)};
    RECT subtitleRect{client.left, center - theme_.px(16), client.right, center + theme_.px(18)};
    RECT hintRect{client.left, center + theme_.px(34), client.right, center + theme_.px(58)};

    const int saved = ::SaveDC(dc);
    ::SetBkMode(dc, TRANSPARENT);

    ::SelectObject(dc, theme_.titleFont());
    ::SetTextColor(dc, ui::ToColorRef(palette.textPrimary));
    ::DrawTextW(dc, L"ActiveXWebMark", -1, &titleRect,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    ::SelectObject(dc, theme_.uiFont());
    ::SetTextColor(dc, ui::ToColorRef(palette.textSecondary));
    ::DrawTextW(dc, L"打开 Markdown 文件开始阅读", -1, &subtitleRect,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    ::SelectObject(dc, theme_.smallFont());
    const std::wstring hint = L"v" + utf8ToWide(version()) +
                              L"  ·  按 Ctrl+O 打开文件  ·  可同时打开多个文件";
    ::DrawTextW(dc, hint.c_str(), -1, &hintRect,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    ::RestoreDC(dc, saved);
}

LRESULT MainWindow::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT MainWindow::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
    info->ptMinTrackSize.x = theme_.px(760);
    info->ptMinTrackSize.y = theme_.px(480);
    return 0;
}

LRESULT MainWindow::OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    theme_.setDpi(LOWORD(wParam));

    auto* suggested = reinterpret_cast<RECT*>(lParam);
    if (suggested != nullptr) {
        ::SetWindowPos(m_hWnd, nullptr, suggested->left, suggested->top,
                       suggested->right - suggested->left, suggested->bottom - suggested->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }

    tabStrip_.OnDpiChanged();
    for (const std::unique_ptr<DocumentView>& view : views_) {
        view->OnDpiChanged();
    }
    Layout();
    return 0;
}

LRESULT MainWindow::OnInitMenuPopup(UINT, WPARAM, LPARAM, BOOL&)
{
    UpdateMenuState();
    return 0;
}

LRESULT MainWindow::OnFileOpen(WORD, WORD, HWND, BOOL&)
{
    const std::vector<std::wstring> paths = ShowOpenMarkdownDialog(m_hWnd);
    OpenFiles(paths);
    return 0;
}

LRESULT MainWindow::OnFileSave(WORD, WORD, HWND, BOOL&)
{
    SaveActive();
    return 0;
}

LRESULT MainWindow::OnFileSaveAs(WORD, WORD, HWND, BOOL&)
{
    SaveActiveAs();
    return 0;
}

LRESULT MainWindow::OnFileCloseTab(WORD, WORD, HWND, BOOL&)
{
    CloseTab(activeIndex_);
    return 0;
}

LRESULT MainWindow::OnFileExit(WORD, WORD, HWND, BOOL&)
{
    ::SendMessageW(m_hWnd, WM_CLOSE, 0, 0);
    return 0;
}

LRESULT MainWindow::OnViewEditor(WORD, WORD, HWND, BOOL&)
{
    viewSettings_.showEditor = !viewSettings_.showEditor;
    ApplyViewSettingsToAll();
    UpdateMenuState();
    if (DocumentView* view = ActiveView()) {
        view->FocusPrimaryPane();
    }
    return 0;
}

LRESULT MainWindow::OnViewNav(WORD, WORD, HWND, BOOL&)
{
    viewSettings_.showNav = !viewSettings_.showNav;
    ApplyViewSettingsToAll();
    UpdateMenuState();
    if (DocumentView* view = ActiveView()) {
        view->FocusPrimaryPane();
    }
    return 0;
}

LRESULT MainWindow::OnViewSplitSideBySide(WORD, WORD, HWND, BOOL&)
{
    viewSettings_.split = ui::Orientation::Horizontal;
    viewSettings_.showEditor = true;
    ApplyViewSettingsToAll();
    UpdateMenuState();
    if (DocumentView* view = ActiveView()) {
        view->FocusPrimaryPane();
    }
    return 0;
}

LRESULT MainWindow::OnViewSplitStacked(WORD, WORD, HWND, BOOL&)
{
    viewSettings_.split = ui::Orientation::Vertical;
    viewSettings_.showEditor = true;
    ApplyViewSettingsToAll();
    UpdateMenuState();
    if (DocumentView* view = ActiveView()) {
        view->FocusPrimaryPane();
    }
    return 0;
}

LRESULT MainWindow::OnViewToggleSplit(WORD, WORD, HWND, BOOL&)
{
    viewSettings_.split = viewSettings_.split == ui::Orientation::Horizontal
                              ? ui::Orientation::Vertical
                              : ui::Orientation::Horizontal;
    viewSettings_.showEditor = true;
    ApplyViewSettingsToAll();
    UpdateMenuState();
    if (DocumentView* view = ActiveView()) {
        view->FocusPrimaryPane();
    }
    return 0;
}

LRESULT MainWindow::OnViewScrollSync(WORD, WORD, HWND, BOOL&)
{
    viewSettings_.scrollSync = !viewSettings_.scrollSync;
    ApplyViewSettingsToAll();
    UpdateMenuState();
    return 0;
}

LRESULT MainWindow::OnRecentFile(WORD, WORD wID, HWND, BOOL&)
{
    const int index = static_cast<int>(wID) - cmd::kRecentBase;
    const std::vector<std::wstring> page = recent_.menuPage();
    if (index >= 0 && index < static_cast<int>(page.size())) {
        OpenFile(page[static_cast<std::size_t>(index)]);
    }
    return 0;
}

LRESULT MainWindow::OnRecentMore(WORD, WORD, HWND, BOOL&)
{
    const std::wstring selected =
        RecentFilesDialog::ShowModal(m_hWnd, recent_.entries(), recent_.pageSize());
    if (!selected.empty()) {
        OpenFile(selected);
    }
    return 0;
}

LRESULT MainWindow::OnHelpAbout(WORD, WORD, HWND, BOOL&)
{
    ShowAboutDialog(m_hWnd);
    return 0;
}

} // namespace awm
