#include "mainwindow.h"

#include <shellapi.h>
#include <windowsx.h>

#include "apppaths.h"

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <utility>

#include "darkmode.h"
#include "dialogs.h"
#include "doublebuffer.h"
#include "fileassociation.h"
#include "filedialogs.h"
#include "recentfilesdialog.h"
#include "resource.h"
#include "scintillaeditor.h"
#include "themeapply.h"
#include "themedialog.h"
#include "theme/theme_store.h"
#include "update/updatedialog.h"
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

const wchar_t* familyLabel(theme::ThemeFamily family)
{
    switch (family) {
    case theme::ThemeFamily::Neutral:
        return L"中性灰";
    case theme::ThemeFamily::Blue:
        return L"蓝色系";
    case theme::ThemeFamily::Cyan:
        return L"青色系";
    case theme::ThemeFamily::Green:
        return L"绿色系";
    case theme::ThemeFamily::Warm:
        return L"暖色系";
    case theme::ThemeFamily::RedPink:
        return L"红粉系";
    case theme::ThemeFamily::Purple:
        return L"紫罗兰";
    case theme::ThemeFamily::Multicolor:
        return L"多彩霓虹";
    }
    return L"其他";
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

bool updateChecksDisabled()
{
    wchar_t value[16] = {};
    const DWORD length =
        ::GetEnvironmentVariableW(L"AWM_DISABLE_UPDATE_CHECK", value, ARRAYSIZE(value));
    return length > 0 && ::_wcsicmp(value, L"0") != 0;
}

bool endsWithNoCase(const std::wstring& text, const wchar_t* suffix)
{
    const std::size_t length = std::wcslen(suffix);
    return text.size() >= length &&
           ::_wcsicmp(text.c_str() + text.size() - length, suffix) == 0;
}

/// The outer frame must stay visible when the preview/tree surface is white,
/// so it is the theme divider nudged toward the text color. On light themes
/// that darkens the line; on dark themes it lightens it.
ui::Color frameBorderColor(const ui::Palette& palette)
{
    constexpr int kTextWeight = 30;
    const auto mix = [kTextWeight](BYTE divider, BYTE text) {
        return static_cast<BYTE>((static_cast<int>(divider) * (100 - kTextWeight) +
                                  static_cast<int>(text) * kTextWeight + 50) /
                                 100);
    };
    return ui::Color{mix(palette.divider.r, palette.textPrimary.r),
                     mix(palette.divider.g, palette.textPrimary.g),
                     mix(palette.divider.b, palette.textPrimary.b)};
}

/// Windows 11 supports Snap Layouts when WM_NCHITTEST returns HTMAXBUTTON.
/// RtlGetVersion is used instead of GetVersionEx so the result ignores the
/// executable's compatibility manifest.
bool isWindows11OrGreater()
{
    static const bool value = []() {
        using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
        const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
            ntdll != nullptr ? ::GetProcAddress(ntdll, "RtlGetVersion") : nullptr);
        if (rtlGetVersion == nullptr) {
            return false;
        }
        OSVERSIONINFOW info = {};
        info.dwOSVersionInfoSize = sizeof(info);
        if (rtlGetVersion(&info) != 0) {
            return false;
        }
        return info.dwMajorVersion > 10 ||
               (info.dwMajorVersion == 10 && info.dwBuildNumber >= 22000);
    }();
    return value;
}

/// Maps a title bar hit to the cursor/non-client code used by WM_NCHITTEST.
int titleBarHitToNcCode(ui::TitleBarHit hit)
{
    switch (hit) {
    case ui::TitleBarHit::Caption:
        return HTCAPTION;
    case ui::TitleBarHit::Maximize:
        // On Windows 11 this enables the Snap Layouts flyout on hover.
        return isWindows11OrGreater() ? HTMAXBUTTON : HTCLIENT;
    case ui::TitleBarHit::Menu:
    case ui::TitleBarHit::Icon:
    case ui::TitleBarHit::Minimize:
    case ui::TitleBarHit::Close:
        return HTCLIENT;
    default:
        return HTCLIENT;
    }
}

/// Builds a safe local file name for a release asset.
std::wstring installerFileName(const ReleaseAsset& asset, const ReleaseInfo& release)
{
    std::wstring name = utf8ToWide(asset.name);
    const std::size_t slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        name = name.substr(slash + 1);
    }

    std::wstring safe;
    safe.reserve(name.size());
    for (const wchar_t ch : name) {
        const bool allowed = (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') ||
                             (ch >= L'a' && ch <= L'z') || ch == L'.' || ch == L'-' ||
                             ch == L'_';
        safe.push_back(allowed ? ch : L'_');
    }

    if (safe.empty() || safe == L"." || safe == L"..") {
        safe = L"ActiveXWebMark-" + utf8ToWide(release.version) + L"-update";
    }
    if (!endsWithNoCase(safe, L".exe") && !endsWithNoCase(safe, L".msi")) {
        safe += endsWithNoCase(utf8ToWide(asset.downloadUrl), L".msi") ? L".msi" : L".exe";
    }
    return safe;
}

bool openReleasePage(const ReleaseInfo& release)
{
    const std::wstring url = utf8ToWide(release.releaseUrl);
    constexpr const wchar_t* kPrefix = L"https://github.com/";
    if (url.size() < std::wcslen(kPrefix) ||
        ::_wcsnicmp(url.c_str(), kPrefix, std::wcslen(kPrefix)) != 0) {
        return false;
    }
    const INT_PTR result = reinterpret_cast<INT_PTR>(
        ::ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}

} // namespace

MainWindow::MainWindow() : registry_(DocumentFormatRegistry::createDefault())
{
    theme_.initialize(96);
    settings_ = createSettings();
    // Must happen before LoadViewSettings(): ui.theme may name a user theme.
    ReloadUserThemes();
    if (settings_ != nullptr) {
        recent_.load(*settings_, "history.recentFiles");
        viewSettings_ = LoadViewSettings(*settings_);
        placement_ = LoadWindowPlacement(*settings_);
        ignoredVersion_ = settings_->get("update.ignoredVersion");
    }
    const theme::ThemeDefinition& initialTheme = theme::ResolveTheme(viewSettings_.theme);
    theme_.setPalette(MakePalette(initialTheme));
    SetProcessColorMode(initialTheme.dark ? ColorMode::Dark : ColorMode::Light);
    previewAssets_ = LoadPreviewAssets(::GetModuleHandleW(nullptr));
    diagrams_ = std::make_unique<DiagramService>();
    if (settings_ != nullptr) {
        diagrams_->SetPlantUmlEnabled(
            settings_->getBool("diagrams.plantuml.enabled", true));
        diagrams_->SetPlantUmlServer(
            settings_->get("diagrams.plantuml.server", "https://kroki.io"));
        diagrams_->SetPlantUmlMode(
            settings_->get("diagrams.plantuml.mode", "auto"));
        diagrams_->SetPlantUmlExe(
            settings_->get("diagrams.plantuml.exe", ""));
    }
    startMaximized_ = placement_.valid && placement_.maximized;
}

MainWindow::~MainWindow()
{
    if (updateDialog_) {
        updateDialog_->CloseSilently();
    }
    updates_.Shutdown();

    if (mainMenu_ != nullptr) {
        ::DestroyMenu(mainMenu_);
        mainMenu_ = nullptr;
        fileMenu_ = nullptr;
        editMenu_ = nullptr;
        viewMenu_ = nullptr;
        themeMenu_ = nullptr;
        themeMenuGroups_.clear();
        helpMenu_ = nullptr;
        recentMenu_ = nullptr;
    }
}

BOOL MainWindow::PreTranslateMessage(MSG* pMsg)
{
    // Tab/Enter/Esc navigation inside the floating update window, and keep the
    // application's global shortcuts from acting while it has focus.
    if (updateDialog_ && updateDialog_->IsWindow() && pMsg->hwnd != nullptr &&
        (pMsg->hwnd == updateDialog_->m_hWnd ||
         ::IsChild(updateDialog_->m_hWnd, pMsg->hwnd))) {
        return ::IsDialogMessageW(updateDialog_->m_hWnd, pMsg) ? TRUE : FALSE;
    }

    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F1) {
        SendCommand(cmd::kHelpAbout);
        return TRUE;
    }

    // Alt / F10 top-level menu navigation (the native menu bar is gone).
    if (HandleMenuKey(pMsg)) {
        return TRUE;
    }

    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_F3) {
            const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            SendCommand(shift ? cmd::kEditFindPrevious : cmd::kEditFindNext);
            return TRUE;
        }
        if (pMsg->wParam == VK_ESCAPE) {
            DocumentView* view = ActiveView();
            if (view != nullptr && view->CloseFindBarIfVisible()) {
                return TRUE;
            }
        }
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
            case L'F':
                SendCommand(cmd::kEditFind);
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
    services.previewAssets = &previewAssets_;
    services.diagrams = diagrams_.get();
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
        appSmallIcon_ = smallIcon;
    }

    titleBar_.SetTheme(&theme_);
    titleBar_.SetIcon(appSmallIcon_);
    titleBar_.SetMaximized(::IsZoomed(m_hWnd) != FALSE);

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
    UpdateWindowTitle();
    Layout();
    RestoreSession();

    // A portable copy has no installer to write the association keys; this is
    // a no-op when the per-machine registration is already present.
    ensureFileAssociationsRegistered();

    if (!updateChecksDisabled()) {
        ::SetTimer(m_hWnd, kStartupUpdateTimer, kStartupUpdateDelayMs, nullptr);
    }
    return 0;
}

void MainWindow::CreateMenuBar()
{
    if (mainMenu_ != nullptr) {
        return;
    }

    mainMenu_ = ::CreateMenu();
    fileMenu_ = ::CreatePopupMenu();
    editMenu_ = ::CreatePopupMenu();
    viewMenu_ = ::CreatePopupMenu();
    themeMenu_ = ::CreatePopupMenu();
    helpMenu_ = ::CreatePopupMenu();
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
    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileSetDefault,
                  L"设为 Markdown 默认打开方式(&D)...");
    ::AppendMenuW(fileMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(fileMenu_, MF_STRING, cmd::kFileExit, L"退出(&X)");

    ::AppendMenuW(editMenu_, MF_STRING, cmd::kEditFind, L"查找(&F)...\tCtrl+F");
    ::AppendMenuW(editMenu_, MF_STRING, cmd::kEditFindNext, L"查找下一个(&N)\tF3");
    ::AppendMenuW(editMenu_, MF_STRING, cmd::kEditFindPrevious, L"查找上一个(&P)\tShift+F3");

    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewEditor, L"显示编辑器(&E)\tCtrl+E");
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewNav, L"显示导航目录(&B)\tCtrl+B");
    ::AppendMenuW(viewMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewSplitSideBySide, L"左右分屏(&L)");
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewSplitStacked, L"上下分屏(&T)");
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewToggleSplit,
                  L"切换分屏方向(&S)\tCtrl+Shift+L");
    ::AppendMenuW(viewMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(viewMenu_, MF_STRING, cmd::kViewScrollSync, L"滚动同步(&Y)");

    BuildThemeMenu();
    ::AppendMenuW(viewMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(viewMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(themeMenu_),
                  L"主题(&Z)");

    ::AppendMenuW(helpMenu_, MF_STRING, cmd::kHelpCheckUpdates, L"检查更新(&U)...");
    ::AppendMenuW(helpMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(helpMenu_, MF_STRING, cmd::kHelpAbout, L"关于 ActiveXWebMark(&A)...\tF1");

    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu_), L"文件(&F)");
    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(editMenu_), L"编辑(&E)");
    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu_), L"视图(&V)");
    ::AppendMenuW(mainMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu_), L"帮助(&H)");

    // The menu is deliberately not attached with SetMenu(): the native menu bar
    // would occupy its own row below the caption. The title bar draws the
    // top-level labels itself and shows the popup menus on demand.
    titleMenus_.clear();
    std::vector<ui::MenuLabel> labels;
    const int menuCount = ::GetMenuItemCount(mainMenu_);
    labels.reserve(menuCount > 0 ? static_cast<std::size_t>(menuCount) : 0);
    for (int i = 0; i < menuCount; ++i) {
        wchar_t text[256] = {};
        ::GetMenuStringW(mainMenu_, static_cast<UINT>(i), text, ARRAYSIZE(text), MF_BYPOSITION);
        labels.push_back(ui::parseMenuLabel(text));
        titleMenus_.push_back(::GetSubMenu(mainMenu_, i));
    }
    titleBar_.SetMenus(std::move(labels));
}

void MainWindow::AppendThemeGroup(HMENU parent, const std::wstring& label,
                                  const std::vector<int>& indices)
{
    HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const std::vector<theme::ThemeDefinition>& themes = theme::AllThemes();
    for (int index : indices) {
        if (index < 0 || static_cast<std::size_t>(index) >= themes.size()) {
            continue;
        }
        const std::wstring item = escapeMenuText(utf8ToWide(themes[index].displayName));
        ::AppendMenuW(menu, MF_STRING,
                      static_cast<UINT_PTR>(cmd::kViewThemeBase) +
                          static_cast<UINT_PTR>(index),
                      item.c_str());
    }

    themeMenuGroups_.push_back(ThemeMenuGroup{menu, indices});
    ::AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(menu), label.c_str());
}

void MainWindow::BuildThemeMenu()
{
    if (themeMenu_ == nullptr) {
        return;
    }

    // Rebuildable: DeleteMenu on popup items destroys their submenus too.
    while (::GetMenuItemCount(themeMenu_) > 0) {
        ::DeleteMenu(themeMenu_, 0, MF_BYPOSITION);
    }
    themeMenuGroups_.clear();

    std::vector<int> custom;
    const std::vector<theme::ThemeDefinition>& all = theme::AllThemes();
    for (std::size_t i = theme::BuiltinThemeCount(); i < all.size(); ++i) {
        custom.push_back(static_cast<int>(i));
    }
    if (!custom.empty()) {
        AppendThemeGroup(themeMenu_, L"自定义主题(&M)", custom);
        ::AppendMenuW(themeMenu_, MF_SEPARATOR, 0, nullptr);
    }

    AppendThemeGroup(themeMenu_, L"浅色主题(&L)", theme::LightThemeIndices());
    AppendThemeGroup(themeMenu_, L"深色主题(&D)", theme::DarkThemeIndices());
    ::AppendMenuW(themeMenu_, MF_SEPARATOR, 0, nullptr);

    HMENU byAuthor = ::CreatePopupMenu();
    if (byAuthor != nullptr) {
        for (const theme::ThemeGroup& group : theme::ThemesGroupedByAuthor()) {
            AppendThemeGroup(byAuthor, escapeMenuText(utf8ToWide(group.label)),
                             group.indices);
        }
        ::AppendMenuW(themeMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(byAuthor),
                      L"按作者/来源(&S)");
    }

    HMENU byFamily = ::CreatePopupMenu();
    if (byFamily != nullptr) {
        for (const theme::FamilyThemeGroup& group : theme::ThemesGroupedByFamily()) {
            AppendThemeGroup(byFamily, familyLabel(group.family), group.indices);
        }
        ::AppendMenuW(themeMenu_, MF_POPUP, reinterpret_cast<UINT_PTR>(byFamily),
                      L"按色系(&C)");
    }

    ::AppendMenuW(themeMenu_, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(themeMenu_, MF_STRING, cmd::kViewThemeSettings, L"主题设置(&S)...");
    themeMenuGeneration_ = theme::CatalogGeneration();
}

void MainWindow::UpdateThemeMenuChecks()
{
    const std::vector<theme::ThemeDefinition>& themes = theme::AllThemes();
    int current = theme::IndexOfTheme(viewSettings_.theme);
    if (current < 0 || static_cast<std::size_t>(current) >= themes.size()) {
        current = 0;
    }
    const UINT checkId = static_cast<UINT>(cmd::kViewThemeBase + current);

    for (const ThemeMenuGroup& group : themeMenuGroups_) {
        if (group.menu == nullptr || group.indices.empty()) {
            continue;
        }

        bool containsCurrent = false;
        for (int index : group.indices) {
            if (index == current) {
                containsCurrent = true;
                break;
            }
        }

        // Clear every item first: a submenu that no longer contains the current
        // theme may still carry a check from a previous selection.
        for (int index : group.indices) {
            ::CheckMenuItem(group.menu,
                            static_cast<UINT>(cmd::kViewThemeBase + index),
                            MF_BYCOMMAND | MF_UNCHECKED);
        }

        if (containsCurrent) {
            // first/last must be items that actually exist in this submenu; a
            // global range would fail on narrow author/family lists.
            const auto bounds =
                std::minmax_element(group.indices.begin(), group.indices.end());
            const UINT first =
                static_cast<UINT>(cmd::kViewThemeBase + *bounds.first);
            const UINT last =
                static_cast<UINT>(cmd::kViewThemeBase + *bounds.second);
            ::CheckMenuRadioItem(group.menu, first, last, checkId, MF_BYCOMMAND);
            ::CheckMenuItem(group.menu, checkId, MF_BYCOMMAND | MF_CHECKED);
        }
    }
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

    // The recent-file items only appear inside the 文件 popup; there is no
    // attached menu bar to repaint.
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
    if (themeMenu_ != nullptr) {
        UpdateThemeMenuChecks();
    }
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
    if (editMenu_ != nullptr) {
        ::EnableMenuItem(editMenu_, cmd::kEditFind, MF_BYCOMMAND | enabled);
        ::EnableMenuItem(editMenu_, cmd::kEditFindNext, MF_BYCOMMAND | enabled);
        ::EnableMenuItem(editMenu_, cmd::kEditFindPrevious, MF_BYCOMMAND | enabled);
    }
    if (helpMenu_ != nullptr) {
        ::EnableMenuItem(helpMenu_, cmd::kHelpCheckUpdates,
                         MF_BYCOMMAND | (updateBusy() ? MF_GRAYED : MF_ENABLED));
    }
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
    titleBar_.SetTitle(title);
    InvalidateTitleBar();
}

void MainWindow::Layout()
{
    if (!::IsWindow(m_hWnd) || !tabStrip_.IsWindow()) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    UpdateTitleBarLayout();
    const int titleHeight = TitleBarHeight();

    if (views_.empty()) {
        tabStrip_.ShowWindow(SW_HIDE);
        Invalidate(FALSE);
        return;
    }

    // Children stop short of the client edge so the frame painted in OnPaint
    // stays visible instead of being covered by the preview or the nav tree.
    const int frame = FrameThickness();
    const int right = std::max(frame, static_cast<int>(client.right) - frame);
    const int bottom = std::max(frame, static_cast<int>(client.bottom) - frame);

    tabStrip_.ShowWindow(SW_SHOW);
    const int stripHeight = tabStrip_.preferredHeight();
    tabStrip_.MoveWindow(frame, titleHeight, std::max(0, right - frame), stripHeight, TRUE);

    RECT content{frame, titleHeight + stripHeight, right, bottom};
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
            // The theme is global; a view's copy may be older than the last
            // theme switch. Preserve the active id rather than letting a
            // splitter drag revert it.
            const std::string themeId = viewSettings_.theme;
            viewSettings_ = settings;
            viewSettings_.theme = themeId;
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
    view->ApplyTheme();
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
    const int frame = FrameThickness();
    const int top = TitleBarHeight() + tabStrip_.preferredHeight();
    view->SetBounds(RECT{frame, top, std::max(frame, static_cast<int>(client.right) - frame),
                         std::max(frame, static_cast<int>(client.bottom) - frame)});
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

void MainWindow::ReloadTab(int index)
{
    if (index < 0 || index >= static_cast<int>(views_.size())) {
        return;
    }

    DocumentView& view = *views_[static_cast<std::size_t>(index)];
    if (!view.session().hasPath()) {
        return;
    }

    if (view.session().isDirty()) {
        switch (AskSaveChanges(m_hWnd, view.session().title())) {
        case SaveDecision::Save:
            if (!SaveView(view)) {
                return;
            }
            break;
        case SaveDecision::Discard:
            break;
        case SaveDecision::Cancel:
        default:
            return;
        }
    }

    const std::wstring path = view.session().path();
    std::error_code ec;
    std::unique_ptr<IDocument> document = registry_.load(path, ec);
    if (!document) {
        std::wstring message = L"无法重新加载文件：\n" + path;
        if (ec) {
            message += L"\n\n" + utf8ToWide(ec.message());
        }
        ShowError(m_hWnd, message);
        return;
    }

    view.Reload(DocumentSession::fromDocument(path, *document));
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
        kReload = 1,
        kClose,
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
    ::AppendMenuW(menu, pathFlags, kReload, L"重新加载(&D)");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
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
    case kReload:
        ReloadTab(index);
        break;
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

void MainWindow::ReloadUserThemes()
{
    theme::ThemeStoreLoadResult loaded = theme::LoadThemeStore(themesDirectory());
    themeLoadErrors_ = std::move(loaded.errors);
    theme::SetUserThemes(theme::DefinitionsOf(loaded.themes));
    // Force the next popup (or explicit rebuild) to use the new indices.
    themeMenuGeneration_ = 0;
}

void MainWindow::ApplyTheme(const theme::ThemeDefinition& definition, bool persist)
{
    viewSettings_.theme = definition.id;
    theme_.setPalette(MakePalette(definition));
    const ColorMode colorMode =
        definition.dark ? ColorMode::Dark : ColorMode::Light;
    SetProcessColorMode(colorMode);
    ApplyColorModeToWindow(m_hWnd, colorMode);

    for (const std::unique_ptr<DocumentView>& view : views_) {
        view->SetViewSettings(viewSettings_);
        view->ApplyTheme();
    }

    titleBar_.SetTheme(&theme_);
    InvalidateTitleBar();
    tabStrip_.SetTheme(&theme_);
    if (updateDialog_ && updateDialog_->IsWindow()) {
        updateDialog_->SetPalette(theme_.palette());
    }

    ::RedrawWindow(m_hWnd, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    UpdateMenuState();

    if (persist && settings_) {
        // JsonFileSettings persists on set(), so the choice survives a crash
        // even though SaveSettings only runs on exit.
        settings_->set("ui.theme", definition.id);
    }
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
    ::KillTimer(m_hWnd, kStartupUpdateTimer);
    ::KillTimer(m_hWnd, kUpdatePollTimer);
    updates_.RequestCancel();
    if (updateDialog_) {
        updateDialog_->CloseSilently();
    }
    checking_ = false;
    downloadActive_ = false;

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
        editMenu_ = nullptr;
        viewMenu_ = nullptr;
        themeMenu_ = nullptr;
        themeMenuGroups_.clear();
        helpMenu_ = nullptr;
        recentMenu_ = nullptr;
    }
    titleMenus_.clear();
    titleBar_.SetIcon(nullptr);

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

    titleBar_.Paint(target);

    if (views_.empty()) {
        RECT content{client.left, static_cast<LONG>(TitleBarHeight()), client.right,
                     client.bottom};
        PaintWelcome(target, content);
    }

    const int frame = FrameThickness();
    if (frame > 0) {
        HBRUSH frameBrush =
            ::CreateSolidBrush(ui::ToColorRef(frameBorderColor(palette)));
        if (frameBrush != nullptr) {
            const LONG right = client.right;
            const LONG bottom = client.bottom;
            RECT strips[] = {
                {client.left, client.top, right, client.top + frame},
                {client.left, bottom - frame, right, bottom},
                {client.left, client.top, client.left + frame, bottom},
                {right - frame, client.top, right, bottom},
            };
            for (const RECT& strip : strips) {
                ::FillRect(target, &strip, frameBrush);
            }
            ::DeleteObject(frameBrush);
        }
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
    const UINT dpi = LOWORD(wParam);
    theme_.setDpi(dpi);

    const int iconWidth = ::GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    const int iconHeight = ::GetSystemMetricsForDpi(SM_CYSMICON, dpi);
    if (HICON icon = static_cast<HICON>(::LoadImageW(
            ::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, iconWidth,
            iconHeight, LR_DEFAULTCOLOR | LR_SHARED))) {
        appSmallIcon_ = icon;
        titleBar_.SetIcon(icon);
        SetIcon(icon, FALSE);
    }

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

int MainWindow::TitleBarHeight() const
{
    return titleBar_.height();
}

int MainWindow::ResizeBorderThickness() const
{
    const UINT dpi = ::GetDpiForWindow(m_hWnd);
    const int frame = ::GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi);
    const int padding = ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    return std::max(4, frame + padding);
}

int MainWindow::FrameThickness() const
{
    // A maximized window sits flush against the work area; a frame there would
    // only draw a ring around the whole screen.
    if (::IsZoomed(m_hWnd)) {
        return 0;
    }
    return std::max(1, theme_.px(1));
}

void MainWindow::UpdateTitleBarLayout()
{
    if (!::IsWindow(m_hWnd)) {
        return;
    }
    RECT client = {};
    GetClientRect(&client);
    titleBar_.Layout(static_cast<int>(client.right));
    // WM_SIZE follows every maximize/restore, so this keeps the maximize/restore
    // glyph in sync even when the state changed without an activation message.
    titleBar_.SetMaximized(::IsZoomed(m_hWnd) != FALSE);
}

void MainWindow::InvalidateTitleBar()
{
    if (!::IsWindow(m_hWnd)) {
        return;
    }
    RECT client = {};
    GetClientRect(&client);
    const RECT bar{client.left, client.top, client.right,
                   client.top + static_cast<LONG>(TitleBarHeight())};
    ::InvalidateRect(m_hWnd, &bar, FALSE);
}

LRESULT MainWindow::OnNcCalcSize(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    // The custom title bar lives in the client area, so the non-client area is
    // collapsed entirely. Resize borders are implemented in WM_NCHITTEST.
    if (wParam == FALSE) {
        return 0;
    }

    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
    if (params != nullptr && ::IsZoomed(m_hWnd)) {
        // A maximized WS_OVERLAPPEDWINDOW is positioned slightly outside the
        // work area so its frame is off-screen. With no NC area, clamp the
        // client to the monitor work area so the taskbar stays visible.
        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        if (::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &info) !=
            FALSE) {
            params->rgrc[0] = info.rcWork;
        }
    }
    return 0;
}

LRESULT MainWindow::OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL&)
{
    const POINT screen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

    if (!::IsZoomed(m_hWnd)) {
        RECT window = {};
        ::GetWindowRect(m_hWnd, &window);
        const int border = ResizeBorderThickness();
        const bool left = screen.x >= window.left && screen.x < window.left + border;
        const bool right = screen.x < window.right && screen.x >= window.right - border;
        const bool top = screen.y >= window.top && screen.y < window.top + border;
        const bool bottom = screen.y < window.bottom && screen.y >= window.bottom - border;

        if (top) {
            return left ? HTTOPLEFT : (right ? HTTOPRIGHT : HTTOP);
        }
        if (bottom) {
            return left ? HTBOTTOMLEFT : (right ? HTBOTTOMRIGHT : HTBOTTOM);
        }
        if (left) {
            return HTLEFT;
        }
        if (right) {
            return HTRIGHT;
        }
    }

    POINT client = screen;
    ::ScreenToClient(m_hWnd, &client);
    RECT clientRect = {};
    ::GetClientRect(m_hWnd, &clientRect);
    if (client.y >= 0 && client.y < TitleBarHeight() && client.x >= 0 &&
        client.x < clientRect.right) {
        int menuIndex = -1;
        return titleBarHitToNcCode(titleBar_.hitTest(client, &menuIndex));
    }

    return DefWindowProc(uMsg, wParam, lParam);
}

LRESULT MainWindow::OnNcActivate(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    const bool active = wParam != FALSE;
    titleBarActive_ = active;
    titleBar_.SetActive(active);
    titleBar_.SetMaximized(::IsZoomed(m_hWnd) != FALSE);
    InvalidateTitleBar();
    // Suppress DefWindowProc's caption painting; the client-drawn title bar is
    // the only title UI.
    return TRUE;
}

LRESULT MainWindow::OnActivate(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    const bool active = LOWORD(wParam) != WA_INACTIVE;
    if (titleBarActive_ != active) {
        titleBarActive_ = active;
        titleBar_.SetActive(active);
        InvalidateTitleBar();
    }
    if (!active) {
        ExitMenuMode();
        titleBar_.CancelPress();
    }
    return DefWindowProc(WM_ACTIVATE, wParam, lParam);
}

LRESULT MainWindow::OnNcLButtonDown(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    if (wParam == HTMAXBUTTON) {
        // Windows 11 returns HTMAXBUTTON for Snap Layouts support; the button
        // still needs to toggle the window itself.
        ::SendMessageW(m_hWnd, WM_SYSCOMMAND,
                       ::IsZoomed(m_hWnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
        return 0;
    }
    return DefWindowProc(WM_NCLBUTTONDOWN, wParam, lParam);
}

LRESULT MainWindow::OnNcMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ::ScreenToClient(m_hWnd, &client);
    if (titleBar_.OnMouseMove(client)) {
        InvalidateTitleBar();
    }
    TRACKMOUSEEVENT track = {};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_NONCLIENT | TME_LEAVE;
    track.hwndTrack = m_hWnd;
    ::TrackMouseEvent(&track);
    return 0;
}

LRESULT MainWindow::OnNcMouseLeave(UINT, WPARAM, LPARAM, BOOL&)
{
    if (titleBar_.OnMouseLeave()) {
        InvalidateTitleBar();
    }
    return 0;
}

LRESULT MainWindow::OnMouseMove(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (client.y >= TitleBarHeight()) {
        if (titleBar_.OnMouseLeave()) {
            InvalidateTitleBar();
        }
        return 0;
    }

    if (titleBar_.OnMouseMove(client)) {
        InvalidateTitleBar();
    }
    if (!titleBarTrackingMouse_) {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = m_hWnd;
        if (::TrackMouseEvent(&track) != FALSE) {
            titleBarTrackingMouse_ = true;
        }
    }
    return 0;
}

LRESULT MainWindow::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&)
{
    titleBarTrackingMouse_ = false;
    if (titleBar_.OnMouseLeave()) {
        InvalidateTitleBar();
    }
    return 0;
}

LRESULT MainWindow::OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL&)
{
    const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const ui::TitleBarHit hit = titleBar_.hitTest(client);
    if (hit == ui::TitleBarHit::None || hit == ui::TitleBarHit::Caption) {
        return DefWindowProc(uMsg, wParam, lParam);
    }

    ExitMenuMode();
    if (titleBar_.OnLButtonDown(client)) {
        InvalidateTitleBar();
    }
    ::SetCapture(m_hWnd);
    return 0;
}

LRESULT MainWindow::OnLButtonUp(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const ui::TitleBarEvent event = titleBar_.OnLButtonUp(client);
    if (::GetCapture() == m_hWnd) {
        ::ReleaseCapture();
    }
    InvalidateTitleBar();
    HandleTitleBarEvent(event);
    return 0;
}

LRESULT MainWindow::OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&)
{
    titleBarTrackingMouse_ = false;
    if (titleBar_.hasPressed()) {
        titleBar_.CancelPress();
        InvalidateTitleBar();
    }
    return 0;
}

void MainWindow::HandleTitleBarEvent(const ui::TitleBarEvent& event)
{
    switch (event.action) {
    case ui::TitleBarAction::OpenMenu:
        ShowTitleMenu(event.menuIndex);
        break;
    case ui::TitleBarAction::Minimize:
        ::SendMessageW(m_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        break;
    case ui::TitleBarAction::ToggleMaximize:
        ::SendMessageW(m_hWnd, WM_SYSCOMMAND,
                       ::IsZoomed(m_hWnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
        break;
    case ui::TitleBarAction::Close:
        ::SendMessageW(m_hWnd, WM_CLOSE, 0, 0);
        break;
    case ui::TitleBarAction::ShowSystemMenu:
        ShowSystemMenu();
        break;
    case ui::TitleBarAction::None:
    default:
        break;
    }
}

void MainWindow::ShowTitleMenu(int index)
{
    if (index < 0 || index >= static_cast<int>(titleMenus_.size())) {
        return;
    }
    HMENU menu = titleMenus_[static_cast<std::size_t>(index)];
    if (menu == nullptr) {
        return;
    }

    ExitMenuMode();
    const RECT label = titleBar_.menuRect(index);
    POINT anchor{label.left, label.bottom};
    ::ClientToScreen(m_hWnd, &anchor);

    titleBar_.SetOpenMenu(index);
    InvalidateTitleBar();
    ::UpdateWindow(m_hWnd);
    ::SetForegroundWindow(m_hWnd);

    const UINT command = ::TrackPopupMenuEx(
        menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, anchor.x,
        anchor.y, m_hWnd, nullptr);

    titleBar_.SetOpenMenu(-1);
    InvalidateTitleBar();
    ::PostMessageW(m_hWnd, WM_NULL, 0, 0);

    if (command != 0) {
        SendCommand(static_cast<int>(command));
    }
}

void MainWindow::ShowSystemMenu()
{
    HMENU menu = ::GetSystemMenu(m_hWnd, FALSE);
    if (menu == nullptr) {
        return;
    }

    const ui::RectI& icon = titleBar_.layout().icon;
    POINT anchor{icon.left(), icon.bottom()};
    ::ClientToScreen(m_hWnd, &anchor);
    ::SetForegroundWindow(m_hWnd);

    const UINT command = ::TrackPopupMenuEx(
        menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, anchor.x, anchor.y, m_hWnd,
        nullptr);
    ::PostMessageW(m_hWnd, WM_NULL, 0, 0);
    if (command != 0) {
        ::PostMessageW(m_hWnd, WM_SYSCOMMAND, command, 0);
    }
}

void MainWindow::EnterMenuMode(bool highlightFirst)
{
    titleBarMenuMode_ = true;
    titleBar_.SetMenuMode(true);
    titleBarMenuHighlight_ = highlightFirst && titleBar_.menuCount() > 0 ? 0 : -1;
    titleBar_.SetHighlightedMenu(titleBarMenuHighlight_);
    InvalidateTitleBar();
}

void MainWindow::ExitMenuMode()
{
    if (!titleBarMenuMode_ && titleBarMenuHighlight_ < 0) {
        return;
    }
    titleBarMenuMode_ = false;
    titleBarMenuHighlight_ = -1;
    titleBar_.SetMenuMode(false);
    titleBar_.SetHighlightedMenu(-1);
    InvalidateTitleBar();
}

bool MainWindow::HandleMenuKey(MSG* message)
{
    if (message == nullptr) {
        return false;
    }

    if (message->message == WM_KEYDOWN && message->wParam == VK_F10) {
        if (titleBarMenuMode_) {
            ExitMenuMode();
        } else {
            EnterMenuMode(true);
        }
        return true;
    }

    if (message->message == WM_SYSKEYDOWN && message->wParam == VK_MENU) {
        if (!titleBarMenuMode_) {
            EnterMenuMode(false);
        }
        // Let the system continue to see Alt so Alt+F4 / Alt+Space still work.
        return false;
    }

    if (message->message == WM_SYSKEYUP && message->wParam == VK_MENU) {
        if (titleBarMenuMode_) {
            ExitMenuMode();
        }
        return false;
    }

    const bool isKey =
        message->message == WM_KEYDOWN || message->message == WM_SYSKEYDOWN;
    if (!isKey) {
        return false;
    }

    if (titleBarMenuMode_ && message->wParam == VK_ESCAPE) {
        ExitMenuMode();
        return true;
    }

    if (titleBarMenuMode_ && titleBar_.menuCount() > 0 &&
        (message->wParam == VK_LEFT || message->wParam == VK_RIGHT)) {
        const int count = titleBar_.menuCount();
        int next = titleBarMenuHighlight_;
        if (next < 0) {
            next = 0;
        } else if (message->wParam == VK_RIGHT) {
            next = (next + 1) % count;
        } else {
            next = (next + count - 1) % count;
        }
        titleBarMenuHighlight_ = next;
        titleBar_.SetHighlightedMenu(next);
        InvalidateTitleBar();
        return true;
    }

    if (titleBarMenuMode_ &&
        (message->wParam == VK_DOWN || message->wParam == VK_RETURN)) {
        ShowTitleMenu(titleBarMenuHighlight_ >= 0 ? titleBarMenuHighlight_ : 0);
        return true;
    }

    if ((message->message == WM_SYSKEYDOWN || titleBarMenuMode_) &&
        message->wParam < 0x10000) {
        const int index =
            titleBar_.mnemonicMenu(static_cast<wchar_t>(message->wParam));
        if (index >= 0) {
            ShowTitleMenu(index);
            return true;
        }
    }
    return false;
}

LRESULT MainWindow::OnTestTitleBarState(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    // Test/automation hover injection (see titlebar-smoke.ps1). The physical
    // cursor cannot be used reliably in automation because TrackMouseEvent
    // clears the hover as soon as the real cursor leaves the title bar.
    if (wParam >= 200 && wParam <= 206) {
        ui::TitleBarHit part = ui::TitleBarHit::None;
        int menuIndex = -1;
        switch (wParam) {
        case 201:
            part = ui::TitleBarHit::Menu;
            menuIndex = static_cast<int>(lParam);
            break;
        case 202:
            part = ui::TitleBarHit::Minimize;
            break;
        case 203:
            part = ui::TitleBarHit::Maximize;
            break;
        case 204:
            part = ui::TitleBarHit::Close;
            break;
        case 205:
            part = ui::TitleBarHit::Caption;
            break;
        case 206:
            part = ui::TitleBarHit::Icon;
            break;
        default:
            break;
        }
        titleBar_.SetHoverForTest(part, menuIndex);
        InvalidateTitleBar();
        ::UpdateWindow(m_hWnd);
        return 0;
    }

    const auto rectValue = [](const ui::RectI& rect, WPARAM part) -> LRESULT {
        switch (part) {
        case 0:
            return rect.x;
        case 1:
            return rect.y;
        case 2:
            return rect.right();
        case 3:
            return rect.bottom();
        default:
            return 0;
        }
    };

    const ui::TitleBarLayout& layout = titleBar_.layout();
    const int menuCount = titleBar_.menuCount();

    if (wParam >= 3 && wParam < static_cast<WPARAM>(3 + 4 * menuCount)) {
        const WPARAM index = (wParam - 3) / 4;
        const WPARAM part = (wParam - 3) % 4;
        return rectValue(layout.menus[static_cast<std::size_t>(index)], part);
    }
    if (wParam >= 100 && wParam < 104) {
        return rectValue(layout.icon, wParam - 100);
    }
    if (wParam >= 104 && wParam < 108) {
        return rectValue(layout.minimizeButton, wParam - 104);
    }
    if (wParam >= 108 && wParam < 112) {
        return rectValue(layout.maximizeButton, wParam - 108);
    }
    if (wParam >= 112 && wParam < 116) {
        return rectValue(layout.closeButton, wParam - 112);
    }
    if (wParam >= 116 && wParam < 120) {
        return rectValue(layout.title, wParam - 116);
    }

    switch (wParam) {
    case 0: {
        LRESULT flags = 0;
        if (titleBarActive_) {
            flags |= 1;
        }
        if (::IsZoomed(m_hWnd)) {
            flags |= 2;
        }
        if (titleBarMenuMode_) {
            flags |= 4;
        }
        return flags;
    }
    case 1:
        return TitleBarHeight();
    case 2:
        return menuCount;
    case 120:
        return titleBar_.hoveredMenu();
    case 121:
        return static_cast<LRESULT>(titleBar_.hoveredPart());
    case 122:
        return static_cast<LRESULT>(titleBar_.pressedPart());
    case 123:
        return titleBar_.openMenu();
    case 124:
        return titleBarMenuMode_ ? 1 : 0;
    case 125:
        return titleBar_.active() ? 1 : 0;
    case 126:
        return titleBarMenuHighlight_;
    case 131:
        return viewSettings_.showEditor ? 1 : 0;
    case 132:
        return viewSettings_.scrollSync ? 1 : 0;
    case 133:
        return viewSettings_.showNav ? 1 : 0;
    case 134:
        return viewSettings_.split == ui::Orientation::Vertical ? 1 : 0;
    case 127:
    case 128:
    case 129:
    case 130: {
        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        if (::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &info) ==
            FALSE) {
            return 0;
        }
        switch (wParam) {
        case 127:
            return info.rcWork.left;
        case 128:
            return info.rcWork.top;
        case 129:
            return info.rcWork.right;
        default:
            return info.rcWork.bottom;
        }
    }
    default:
        return 0;
    }
}

LRESULT MainWindow::OnInitMenuPopup(UINT, WPARAM, LPARAM, BOOL&)
{
    // The catalog can change after importing/deleting a theme. Rebuild before
    // showing the menu so command ids still match catalog indices.
    if (themeMenu_ != nullptr && theme::CatalogGeneration() != themeMenuGeneration_) {
        BuildThemeMenu();
    }
    UpdateMenuState();
    return 0;
}

LRESULT MainWindow::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    const UINT_PTR timerId = wParam;
    if (timerId == kStartupUpdateTimer) {
        ::KillTimer(m_hWnd, kStartupUpdateTimer);
        if (updateChecksDisabled()) {
            return 0;
        }

        // Do not interrupt a modal dialog that is already open.
        const HWND popup = ::GetLastActivePopup(m_hWnd);
        if (popup != nullptr && popup != m_hWnd) {
            ::SetTimer(m_hWnd, kStartupUpdateTimer, 3000, nullptr);
            return 0;
        }

        StartUpdateCheck(false);
        return 0;
    }

    if (timerId == kUpdatePollTimer) {
        PollUpdate();
        return 0;
    }
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

LRESULT MainWindow::OnFileSetDefault(WORD, WORD, HWND, BOOL&)
{
    if (!ensureFileAssociationsRegistered()) {
        ShowError(m_hWnd, L"无法写入文件关联注册表，请检查当前用户的注册表权限。");
        return 0;
    }

    // Windows 10 ignores programmatic default-app changes, so the user has to
    // confirm the choice in the system settings page.
    openDefaultAppsSettings();
    ShowInfo(m_hWnd,
             L"已将 ActiveXWebMark 登记为 Markdown 文件的打开方式。\n\n"
             L"Windows 不允许程序自动修改默认应用。请在打开的“默认应用”设置页中，"
             L"将 .md 文件的默认程序选择为 ActiveXWebMark。");
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

LRESULT MainWindow::OnViewTheme(WORD, WORD wID, HWND, BOOL&)
{
    const int index = static_cast<int>(wID) - cmd::kViewThemeBase;
    const std::vector<theme::ThemeDefinition>& themes = theme::AllThemes();
    if (index < 0 || index >= static_cast<int>(themes.size())) {
        return 0;
    }
    ApplyTheme(themes[static_cast<std::size_t>(index)], true);
    return 0;
}

LRESULT MainWindow::OnViewThemeSettings(WORD, WORD, HWND, BOOL&)
{
    const ThemeDialogResult result = ThemeDialog::ShowModal(
        m_hWnd, viewSettings_.theme, themesDirectory(), &previewAssets_, theme_.palette());

    if (result.catalogChanged) {
        ReloadUserThemes();
        BuildThemeMenu();
    }

    bool applied = false;
    if (result.applyRequested && !result.selectedThemeId.empty() &&
        theme::FindTheme(result.selectedThemeId) != nullptr) {
        ApplyTheme(theme::ResolveTheme(result.selectedThemeId), true);
        applied = true;
    } else if (!result.savedThemeId.empty() &&
               result.savedThemeId == viewSettings_.theme &&
               theme::FindTheme(result.savedThemeId) != nullptr) {
        // Saved edits to the currently active theme: refresh the existing
        // surfaces without switching to a different theme.
        ApplyTheme(theme::ResolveTheme(result.savedThemeId), true);
        applied = true;
    }
    if (!applied && theme::FindTheme(viewSettings_.theme) == nullptr) {
        ApplyTheme(theme::DefaultTheme(), true);
    }
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
        RecentFilesDialog::ShowModal(m_hWnd, recent_.entries(), recent_.pageSize(),
                                     theme_.palette());
    if (!selected.empty()) {
        OpenFile(selected);
    }
    return 0;
}

LRESULT MainWindow::OnEditFind(WORD, WORD, HWND, BOOL&)
{
    if (DocumentView* view = ActiveView()) {
        // Ctrl+F / the Edit menu toggles the find bar.
        if (!view->CloseFindBarIfVisible()) {
            view->ShowFindBar();
        }
    }
    return 0;
}

LRESULT MainWindow::OnEditFindNext(WORD, WORD, HWND, BOOL&)
{
    if (DocumentView* view = ActiveView()) {
        view->FindNext();
    }
    return 0;
}

LRESULT MainWindow::OnEditFindPrevious(WORD, WORD, HWND, BOOL&)
{
    if (DocumentView* view = ActiveView()) {
        view->FindPrevious();
    }
    return 0;
}

LRESULT MainWindow::OnHelpCheckUpdates(WORD, WORD, HWND, BOOL&)
{
    if (updateBusy()) {
        if (updateDialog_ && updateDialog_->IsOpen()) {
            ::SetForegroundWindow(updateDialog_->m_hWnd);
        }
        return 0;
    }

    if (updateDialog_ && updateDialog_->IsOpen()) {
        updateDialog_->CloseSilently();
    }
    StartUpdateCheck(true);
    return 0;
}

bool MainWindow::updateBusy() const
{
    return checking_ || downloadActive_ || updates_.IsBusy();
}

void MainWindow::EnsureUpdatePolling()
{
    ::SetTimer(m_hWnd, kUpdatePollTimer, kUpdatePollIntervalMs, nullptr);
}

void MainWindow::StopUpdatePolling()
{
    ::KillTimer(m_hWnd, kUpdatePollTimer);
}

bool MainWindow::EnsureUpdateDialog()
{
    if (!updateDialog_) {
        updateDialog_ = std::make_unique<UpdateDialog>();
        updateDialog_->SetCallbacks(UpdateDialog::Callbacks{
            [this]() { DismissUpdateDialog(); },
            [this]() { InstallPreparedUpdate(); },
            [this]() { RetryPreparedDownload(); }});
        updateDialog_->SetPalette(theme_.palette());
    }

    if (!updateDialog_->IsOpen() && !updateDialog_->Create(m_hWnd)) {
        return false;
    }
    updateDialog_->SetPalette(theme_.palette());
    return true;
}

void MainWindow::StartUpdateCheck(bool manual)
{
    if (updateBusy()) {
        return;
    }

    latestRelease_.reset();
    checkIsStartup_ = !manual;
    checkDismissed_ = false;
    checking_ = true;

    if (manual && EnsureUpdateDialog()) {
        updateDialog_->ShowChecking(utf8ToWide(version()));
    }

    if (!updates_.StartCheck(version())) {
        checking_ = false;
        StopUpdatePolling();
        if (updateDialog_ && updateDialog_->IsOpen() &&
            updateDialog_->mode() == UpdateDialog::Mode::Checking) {
            updateDialog_->CloseSilently();
        }
        if (manual) {
            ShowError(m_hWnd, L"另一个更新任务正在进行中。");
        }
        UpdateMenuState();
        return;
    }

    EnsureUpdatePolling();
    UpdateMenuState();
}

void MainWindow::PollUpdate()
{
    if (checking_) {
        std::optional<CheckResult> result = updates_.TakeCheckResult();
        if (result) {
            checking_ = false;
            StopUpdatePolling();
            HandleCheckResult(std::move(*result));
            if (checking_ || downloadActive_) {
                EnsureUpdatePolling();
            }
        } else if (!updates_.IsBusy()) {
            checking_ = false;
            StopUpdatePolling();
            UpdateMenuState();
        }
        return;
    }

    if (downloadActive_) {
        const UpdateService::DownloadSnapshot snapshot = updates_.DownloadProgress();
        if (updateDialog_ && updateDialog_->IsOpen() &&
            updateDialog_->mode() == UpdateDialog::Mode::Downloading) {
            updateDialog_->SetProgress(snapshot.downloaded, snapshot.total);
        }

        std::optional<DownloadResult> result = updates_.TakeDownloadResult();
        if (result) {
            downloadActive_ = false;
            StopUpdatePolling();
            HandleDownloadResult(std::move(*result));
            if (downloadActive_) {
                EnsureUpdatePolling();
            }
        } else if (!updates_.IsBusy()) {
            downloadActive_ = false;
            StopUpdatePolling();
            UpdateMenuState();
        }
        return;
    }

    StopUpdatePolling();
}

void MainWindow::HandleCheckResult(CheckResult result)
{
    const bool manual = !checkIsStartup_;
    const bool dismissed = checkDismissed_;
    checkIsStartup_ = true;
    checkDismissed_ = false;

    if (updateDialog_ && updateDialog_->IsOpen() &&
        updateDialog_->mode() == UpdateDialog::Mode::Checking) {
        updateDialog_->CloseSilently();
    }

    if (dismissed || result.status == CheckStatus::Cancelled) {
        UpdateMenuState();
        return;
    }

    if (result.status == CheckStatus::NoRelease) {
        if (manual) {
            ShowInfo(m_hWnd, L"尚未发布可用的版本。");
        }
        UpdateMenuState();
        return;
    }

    if (result.status != CheckStatus::Ok) {
        if (manual) {
            ShowError(m_hWnd, result.error.empty()
                                  ? L"检查更新失败。"
                                  : utf8ToWide(result.error));
        }
        UpdateMenuState();
        return;
    }

    if (!result.hasUpdate) {
        if (manual) {
            ShowInfo(m_hWnd, L"当前版本 v" + utf8ToWide(version()) + L" 已是最新版本。");
        }
        UpdateMenuState();
        return;
    }

    latestRelease_ = std::move(result.release);
    if (!manual && !ignoredVersion_.empty() &&
        ignoredVersion_ == latestRelease_->version) {
        UpdateMenuState();
        return;
    }

    const std::wstring currentVersion = utf8ToWide(version());
    const bool hasInstaller = findInstallerAsset(*latestRelease_) != nullptr;
    switch (AskUpdatePrompt(m_hWnd, currentVersion, *latestRelease_, hasInstaller)) {
    case UpdatePromptChoice::Install:
        if (hasInstaller) {
            StartDownloadForRelease(*latestRelease_);
        } else if (!openReleasePage(*latestRelease_)) {
            ShowError(m_hWnd, L"无法打开 GitHub 发布页面。");
        }
        break;
    case UpdatePromptChoice::OpenPage:
        if (!openReleasePage(*latestRelease_)) {
            ShowError(m_hWnd, L"无法打开 GitHub 发布页面。");
        }
        break;
    case UpdatePromptChoice::Ignore:
        ignoredVersion_ = latestRelease_->version;
        if (settings_ != nullptr) {
            settings_->set("update.ignoredVersion", ignoredVersion_);
        }
        break;
    case UpdatePromptChoice::Later:
    default:
        break;
    }
    UpdateMenuState();
}

void MainWindow::StartDownloadForRelease(const ReleaseInfo& release)
{
    if (downloadActive_ || updates_.IsBusy()) {
        return;
    }

    latestRelease_ = release;
    const ReleaseAsset* asset = findInstallerAsset(release);
    if (asset == nullptr) {
        if (!openReleasePage(release)) {
            ShowError(m_hWnd, L"此版本没有提供可下载的安装包。");
        }
        return;
    }

    if (preparedInstallerVersion_ == release.version &&
        !preparedInstallerPath_.empty()) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(preparedInstallerPath_, ec)) {
            if (EnsureUpdateDialog()) {
                updateDialog_->ShowReady(utf8ToWide(release.version));
            }
            UpdateMenuState();
            return;
        }
        preparedInstallerPath_.clear();
        preparedInstallerVersion_.clear();
    }

    const std::filesystem::path directory = updateDirectory();
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        ShowError(m_hWnd, L"无法创建更新下载目录：\n" + directory.wstring() + L"\n\n" +
                              utf8ToWide(ec.message()));
        return;
    }

    const std::wstring target =
        (directory / installerFileName(*asset, release)).wstring();
    if (!EnsureUpdateDialog()) {
        ShowError(m_hWnd, L"无法打开更新下载窗口。");
        return;
    }

    preparedInstallerPath_.clear();
    preparedInstallerVersion_.clear();
    updateDialog_->ShowDownloading(utf8ToWide(release.version), utf8ToWide(asset->name),
                                   asset->size);

    if (!updates_.StartDownload(*asset, target)) {
        updateDialog_->ShowError(L"另一个更新任务正在进行中。");
        return;
    }

    downloadActive_ = true;
    EnsureUpdatePolling();
    UpdateMenuState();
}

void MainWindow::HandleDownloadResult(DownloadResult result)
{
    const bool dialogOpen = updateDialog_ && updateDialog_->IsOpen();

    if (result.cancelled) {
        if (dialogOpen && updateDialog_->mode() == UpdateDialog::Mode::Downloading) {
            updateDialog_->CloseSilently();
        }
        UpdateMenuState();
        return;
    }

    if (!result.success) {
        if (dialogOpen && updateDialog_->mode() == UpdateDialog::Mode::Downloading) {
            updateDialog_->ShowError(utf8ToWide(result.error.empty() ? "下载失败。"
                                                                     : result.error));
        }
        UpdateMenuState();
        return;
    }

    preparedInstallerPath_ = result.path;
    preparedInstallerVersion_ =
        latestRelease_ ? latestRelease_->version : std::string{};

    if (dialogOpen) {
        updateDialog_->ShowReady(utf8ToWide(preparedInstallerVersion_));
    } else {
        ShowInfo(m_hWnd, L"新版本已下载完成。可从“帮助 → 检查更新”安装。");
    }
    UpdateMenuState();
}

void MainWindow::DismissUpdateDialog()
{
    if (!updateDialog_ || !updateDialog_->IsOpen()) {
        return;
    }

    switch (updateDialog_->mode()) {
    case UpdateDialog::Mode::Checking:
        checkDismissed_ = true;
        updates_.RequestCancel();
        break;
    case UpdateDialog::Mode::Downloading:
        updates_.RequestCancel();
        break;
    case UpdateDialog::Mode::Ready:
        // Keep the verified installer for a later installation.
        break;
    case UpdateDialog::Mode::Error:
    default:
        break;
    }
    UpdateMenuState();
}

void MainWindow::InstallPreparedUpdate()
{
    std::error_code ec;
    const bool installerMissing =
        preparedInstallerPath_.empty() ||
        !std::filesystem::is_regular_file(preparedInstallerPath_, ec);
    if (installerMissing) {
        preparedInstallerPath_.clear();
        preparedInstallerVersion_.clear();
        if (updateDialog_ && updateDialog_->IsOpen()) {
            updateDialog_->ShowError(L"安装包已不存在，请重新下载。");
        } else {
            ShowError(m_hWnd, L"安装包已不存在，请重新检查更新。");
        }
        UpdateMenuState();
        return;
    }

    // Saving or discarding every open document happens here; a Cancel leaves
    // the update window open so the user can install later.
    if (!ConfirmCloseAll()) {
        return;
    }

    if (!LaunchInstaller(preparedInstallerPath_)) {
        ShowError(m_hWnd, L"无法启动安装程序：\n" + preparedInstallerPath_);
        return;
    }

    // WM_CLOSE saves the session and settings, then shuts the process down.
    // LaunchInstaller waits briefly before starting the setup, so the
    // installer can replace the executable.
    ::PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
}

void MainWindow::RetryPreparedDownload()
{
    if (latestRelease_) {
        StartDownloadForRelease(*latestRelease_);
    }
}

LRESULT MainWindow::OnHelpAbout(WORD, WORD, HWND, BOOL&)
{
    ShowAboutDialog(m_hWnd);
    return 0;
}

} // namespace awm
