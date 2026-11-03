#include "updatedialog.h"

#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <utility>

namespace awm {

namespace {

std::wstring formatBytes(long long bytes)
{
    if (bytes < 0) {
        bytes = 0;
    }

    constexpr long long kUnit = 1024;
    const wchar_t* names[] = {L"B", L"KB", L"MB", L"GB"};

    int unit = 0;
    long long divisor = 1;
    while (unit < 3 && bytes >= divisor * kUnit) {
        divisor *= kUnit;
        ++unit;
    }

    wchar_t buffer[64] = {};
    if (unit == 0) {
        _snwprintf_s(buffer, ARRAYSIZE(buffer), _TRUNCATE, L"%lld %s", bytes, names[unit]);
    } else {
        const double value = static_cast<double>(bytes) / static_cast<double>(divisor);
        _snwprintf_s(buffer, ARRAYSIZE(buffer), _TRUNCATE, L"%.1f %s", value, names[unit]);
    }
    return buffer;
}

} // namespace

UpdateDialog::~UpdateDialog()
{
    if (IsWindow()) {
        DestroyWindow();
    }
}

bool UpdateDialog::Create(HWND owner)
{
    theme_.initialize(::GetDpiForWindow(owner));

    const int width = theme_.px(480);
    const int height = theme_.px(236);

    RECT ownerRect = {};
    ::GetWindowRect(owner, &ownerRect);

    int x = ownerRect.right - width - theme_.px(28);
    int y = ownerRect.bottom - height - theme_.px(64);

    RECT work = {};
    if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        const int margin = theme_.px(8);
        x = std::max(static_cast<int>(work.left) + margin,
                     std::min(x, static_cast<int>(work.right) - width - margin));
        y = std::max(static_cast<int>(work.top) + margin,
                     std::min(y, static_cast<int>(work.bottom) - height - margin));
    }

    RECT rect{x, y, x + width, y + height};
    const HWND created = CWindowImpl<UpdateDialog>::Create(
        owner, rect, L"软件更新", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT);
    return created != nullptr;
}

void UpdateDialog::SetCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

void UpdateDialog::ShowChecking(const std::wstring& currentVersion)
{
    if (!IsWindow()) {
        return;
    }

    mode_ = Mode::Checking;
    title_.SetWindowText(L"正在检查更新…");
    status_.SetWindowText(L"正在连接 GitHub 获取最新版本信息…");

    const std::wstring detail =
        currentVersion.empty() ? L"请稍候…" : L"当前版本 v" + currentVersion;
    detail_.SetWindowText(detail.c_str());

    setIndeterminate(true);
    setButton(cancelButton_, true, L"取消");
    setButton(installButton_, false, nullptr);
    setButton(retryButton_, false, nullptr);
    showMode();
    cancelButton_.SetFocus();
}

void UpdateDialog::ShowDownloading(const std::wstring& version, const std::wstring& assetName,
                                   long long expectedBytes)
{
    if (!IsWindow()) {
        return;
    }

    mode_ = Mode::Downloading;
    title_.SetWindowText(L"正在下载更新");

    std::wstring headline =
        version.empty() ? L"正在下载安装包…" : L"ActiveXWebMark " + version;
    if (!assetName.empty()) {
        headline += L"\n" + assetName;
    }
    status_.SetWindowText(headline.c_str());

    if (expectedBytes > 0) {
        setIndeterminate(false);
        setDeterminateProgress(0, expectedBytes);
    } else {
        setIndeterminate(true);
        detail_.SetWindowText(L"正在连接下载服务器…");
    }

    setButton(cancelButton_, true, L"取消下载");
    setButton(installButton_, false, nullptr);
    setButton(retryButton_, false, nullptr);
    showMode();
    cancelButton_.SetFocus();
}

void UpdateDialog::SetProgress(long long downloaded, long long total)
{
    if (!IsWindow() || mode_ != Mode::Downloading) {
        return;
    }

    if (total > 0) {
        setDeterminateProgress(downloaded, total);
    } else {
        setIndeterminate(true);
        const std::wstring detail = L"已下载 " + formatBytes(downloaded);
        detail_.SetWindowText(detail.c_str());
    }
}

void UpdateDialog::ShowReady(const std::wstring& version)
{
    if (!IsWindow()) {
        return;
    }

    mode_ = Mode::Ready;
    title_.SetWindowText(L"更新已准备好安装");

    const std::wstring status = version.empty()
                                    ? L"新版本的安装包已下载完成。"
                                    : L"ActiveXWebMark " + version + L" 已下载完成。";
    status_.SetWindowText(status.c_str());
    detail_.SetWindowText(L"点击“立即安装”将关闭本程序并启动安装程序。");

    setIndeterminate(false);
    ::SendMessageW(progress_.m_hWnd, PBM_SETSTATE, PBST_NORMAL, 0);
    ::SendMessageW(progress_.m_hWnd, PBM_SETRANGE32, 0, 1000);
    ::SendMessageW(progress_.m_hWnd, PBM_SETPOS, 1000, 0);

    setButton(cancelButton_, true, L"稍后");
    setButton(installButton_, true, L"立即安装");
    setButton(retryButton_, false, nullptr);
    showMode();
    installButton_.SetFocus();
}

void UpdateDialog::ShowError(const std::wstring& message)
{
    if (!IsWindow()) {
        return;
    }

    mode_ = Mode::Error;
    title_.SetWindowText(L"下载失败");
    status_.SetWindowText(message.empty() ? L"下载更新时发生错误。" : message.c_str());
    detail_.SetWindowText(L"请检查网络连接后重试，或从 GitHub 发布页面手动下载。");

    setErrorState();
    setButton(cancelButton_, true, L"关闭");
    setButton(installButton_, false, nullptr);
    setButton(retryButton_, true, L"重试下载");
    showMode();
    retryButton_.SetFocus();
}

void UpdateDialog::CloseSilently()
{
    if (IsWindow()) {
        DestroyWindow();
    }
}

LRESULT UpdateDialog::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
    closing_ = false;
    theme_.initialize(::GetDpiForWindow(m_hWnd));
    backgroundBrush_ = ::CreateSolidBrush(ui::ToColorRef(theme_.palette().windowBg));
    createChildren();
    return 0;
}

void UpdateDialog::createChildren()
{
    RECT zero = {0, 0, 0, 0};

    title_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                  kTitleId);
    status_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                   kStatusId);
    detail_.Create(m_hWnd, zero, L"",
                   WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0,
                   kDetailId);

    progress_.Create(m_hWnd, zero, nullptr,
                     WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, kProgressId);
    ::SendMessageW(progress_.m_hWnd, PBM_SETRANGE32, 0, 1000);

    cancelButton_.Create(m_hWnd, zero, L"取消",
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, kCancelId);
    installButton_.Create(m_hWnd, zero, L"立即安装",
                          WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, kInstallId);
    retryButton_.Create(m_hWnd, zero, L"重试下载",
                        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, kRetryId);

    applyFonts();
    layoutChildren();
}

void UpdateDialog::applyFonts()
{
    if (!IsWindow()) {
        return;
    }

    const auto setFont = [](HWND control, HFONT font) {
        if (control != nullptr) {
            ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
    };

    setFont(title_.m_hWnd, theme_.uiFontSemibold());
    setFont(status_.m_hWnd, theme_.uiFont());
    setFont(detail_.m_hWnd, theme_.smallFont());
    setFont(cancelButton_.m_hWnd, theme_.uiFont());
    setFont(installButton_.m_hWnd, theme_.uiFont());
    setFont(retryButton_.m_hWnd, theme_.uiFont());
}

void UpdateDialog::layoutChildren()
{
    if (!IsWindow() || !title_.IsWindow()) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);

    const int margin = theme_.px(18);
    const int gap = theme_.px(8);
    const int buttonHeight = theme_.px(30);
    const int contentWidth = std::max(0, static_cast<int>(client.right) - margin * 2);

    int top = margin;
    title_.MoveWindow(margin, top, contentWidth, theme_.px(22), TRUE);
    top += theme_.px(22) + theme_.px(6);
    status_.MoveWindow(margin, top, contentWidth, theme_.px(38), TRUE);
    top += theme_.px(38) + theme_.px(10);
    progress_.MoveWindow(margin, top, contentWidth, theme_.px(14), TRUE);
    top += theme_.px(14) + theme_.px(6);
    detail_.MoveWindow(margin, top, contentWidth, theme_.px(18), TRUE);

    const int buttonTop = client.bottom - margin - buttonHeight;
    int right = client.right - margin;

    const auto place = [&](CButton& button, int width) {
        if (!button.IsWindow() || (button.GetStyle() & WS_VISIBLE) == 0) {
            return;
        }
        right -= width;
        button.MoveWindow(right, buttonTop, width, buttonHeight, TRUE);
        right -= gap;
    };

    place(installButton_, theme_.px(110));
    place(retryButton_, theme_.px(100));
    place(cancelButton_, theme_.px(100));
}

void UpdateDialog::showMode()
{
    if (!IsWindow()) {
        return;
    }
    layoutChildren();
    ShowWindow(SW_SHOW);
    ::SetForegroundWindow(m_hWnd);
}

void UpdateDialog::closeWith(const std::function<void()>& callback)
{
    if (closing_) {
        return;
    }
    closing_ = true;
    if (callback) {
        callback();
    }
    DestroyWindow();
}

void UpdateDialog::setButton(CButton& button, bool visible, const wchar_t* text)
{
    if (!button.IsWindow()) {
        return;
    }
    if (text != nullptr) {
        button.SetWindowText(text);
    }
    button.ShowWindow(visible ? SW_SHOW : SW_HIDE);
}

void UpdateDialog::setIndeterminate(bool indeterminate)
{
    if (!progress_.IsWindow()) {
        return;
    }
    if (indeterminate) {
        // The control has no length to report yet; show an empty, healthy bar
        // and let the status text carry the "working" state.
        ::SendMessageW(progress_.m_hWnd, PBM_SETSTATE, PBST_NORMAL, 0);
        ::SendMessageW(progress_.m_hWnd, PBM_SETRANGE32, 0, 1000);
        ::SendMessageW(progress_.m_hWnd, PBM_SETPOS, 0, 0);
    }
}

void UpdateDialog::setDeterminateProgress(long long downloaded, long long total)
{
    if (!progress_.IsWindow()) {
        return;
    }

    setIndeterminate(false);
    ::SendMessageW(progress_.m_hWnd, PBM_SETSTATE, PBST_NORMAL, 0);
    ::SendMessageW(progress_.m_hWnd, PBM_SETRANGE32, 0, 1000);

    int position = 0;
    int percent = 0;
    if (total > 0) {
        const double fraction =
            static_cast<double>(std::max(0LL, downloaded)) / static_cast<double>(total);
        position = std::clamp(static_cast<int>(fraction * 1000.0 + 0.5), 0, 1000);
        percent = std::clamp(static_cast<int>(fraction * 100.0 + 0.5), 0, 100);
    }
    ::SendMessageW(progress_.m_hWnd, PBM_SETPOS, position, 0);

    std::wstring text = L"已下载 " + formatBytes(downloaded);
    if (total > 0) {
        text += L" / " + formatBytes(total) + L"（" + std::to_wstring(percent) + L"%）";
    }
    detail_.SetWindowText(text.c_str());
}

void UpdateDialog::setErrorState()
{
    if (!progress_.IsWindow()) {
        return;
    }
    ::SendMessageW(progress_.m_hWnd, PBM_SETSTATE, PBST_ERROR, 0);
}

LRESULT UpdateDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    closing_ = true;

    // This dialog object is reused for later checks/downloads. The ATL control
    // wrappers do not clear their own handles when the parent is destroyed, and
    // ATL asserts (or __assume's in Release) that a wrapper being Created has a
    // null handle. Reset them so the next Create() starts from a clean slate.
    title_.m_hWnd = nullptr;
    status_.m_hWnd = nullptr;
    detail_.m_hWnd = nullptr;
    progress_.m_hWnd = nullptr;
    cancelButton_.m_hWnd = nullptr;
    installButton_.m_hWnd = nullptr;
    retryButton_.m_hWnd = nullptr;

    if (backgroundBrush_ != nullptr) {
        ::DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
    return 0;
}

LRESULT UpdateDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    closeWith(callbacks_.onCancel);
    return 0;
}

LRESULT UpdateDialog::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    layoutChildren();
    return 0;
}

LRESULT UpdateDialog::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);
    RECT client = {};
    GetClientRect(&client);
    if (backgroundBrush_ != nullptr) {
        ::FillRect(dc, &client, backgroundBrush_);
    }
    EndPaint(&ps);
    return 0;
}

LRESULT UpdateDialog::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT UpdateDialog::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    HDC dc = reinterpret_cast<HDC>(wParam);
    const HWND control = reinterpret_cast<HWND>(lParam);
    const ui::Palette& palette = theme_.palette();

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ui::ToColorRef(control == detail_.m_hWnd ? palette.textSecondary
                                                               : palette.textPrimary));
    return reinterpret_cast<LRESULT>(backgroundBrush_);
}

LRESULT UpdateDialog::OnDpiChangedMessage(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    theme_.setDpi(LOWORD(wParam));
    applyFonts();

    const auto* suggested = reinterpret_cast<const RECT*>(lParam);
    if (suggested != nullptr) {
        ::SetWindowPos(m_hWnd, nullptr, suggested->left, suggested->top,
                       suggested->right - suggested->left, suggested->bottom - suggested->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
    layoutChildren();
    Invalidate(TRUE);
    return 0;
}

LRESULT UpdateDialog::OnCancelButton(WORD, WORD, HWND, BOOL&)
{
    closeWith(callbacks_.onCancel);
    return 0;
}

LRESULT UpdateDialog::OnInstallButton(WORD, WORD, HWND, BOOL&)
{
    if (mode_ == Mode::Ready && callbacks_.onInstall) {
        callbacks_.onInstall();
    }
    return 0;
}

LRESULT UpdateDialog::OnRetryButton(WORD, WORD, HWND, BOOL&)
{
    if (mode_ == Mode::Error && callbacks_.onRetry) {
        callbacks_.onRetry();
    }
    return 0;
}

} // namespace awm
