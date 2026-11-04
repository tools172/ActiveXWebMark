#include "dialogs.h"

#include <commctrl.h>
#include <shellapi.h>

#include <cctype>
#include <cstdio>
#include <cwchar>

#include "resource.h"
#include "util/stringutil.h"
#include "version.h"

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

bool isGitHubHttpsUrl(const std::wstring& url)
{
    constexpr const wchar_t* kPrefix = L"https://github.com/";
    const std::size_t prefixLength = std::wcslen(kPrefix);
    return url.size() >= prefixLength &&
           ::_wcsnicmp(url.c_str(), kPrefix, prefixLength) == 0;
}

std::wstring escapeHtmlAttribute(const std::wstring& text)
{
    std::wstring escaped;
    escaped.reserve(text.size());
    for (const wchar_t ch : text) {
        if (ch == L'&') {
            escaped += L"&amp;";
        } else if (ch == L'"') {
            escaped += L"&quot;";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::wstring publishedDate(const std::string& timestamp)
{
    if (timestamp.size() < 10) {
        return {};
    }
    const std::string date = timestamp.substr(0, 10);
    if (date[4] != '-' || date[7] != '-') {
        return {};
    }
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i != 4 && i != 7 && std::isdigit(static_cast<unsigned char>(date[i])) == 0) {
            return {};
        }
    }
    return utf8ToWide(date);
}

// Opens the release-page hyperlink in the update prompt. The callback only
// follows GitHub HTTPS links, so content text cannot navigate anywhere else.
HRESULT CALLBACK UpdateHyperlinkCallback(HWND /*window*/, UINT notification,
                                         WPARAM /*wParam*/, LPARAM lParam,
                                         LONG_PTR /*refData*/)
{
    if (notification == TDN_HYPERLINK_CLICKED) {
        const auto* url = reinterpret_cast<const wchar_t*>(lParam);
        if (url != nullptr && isGitHubHttpsUrl(url)) {
            ::ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
    return S_OK;
}

// Opens the hyperlink shown in the About dialog's footer.
HRESULT CALLBACK AboutHyperlinkCallback(HWND /*window*/, UINT notification, WPARAM /*wParam*/,
                                        LPARAM lParam, LONG_PTR /*refData*/)
{
    if (notification == TDN_HYPERLINK_CLICKED) {
        const auto* url = reinterpret_cast<const wchar_t*>(lParam);
        if (url != nullptr) {
            ::ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
    return S_OK;
}

int showTaskDialog(HWND owner, const wchar_t* mainIcon, const wchar_t* instruction,
                   const std::wstring& content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons,
                   const wchar_t* windowTitle)
{
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = windowTitle;
    config.pszMainIcon = mainIcon;
    config.pszMainInstruction = instruction;
    config.pszContent = content.c_str();
    config.dwCommonButtons = buttons;

    int button = 0;
    if (FAILED(::TaskDialogIndirect(&config, &button, nullptr, nullptr))) {
        UINT fallback = MB_OK | MB_ICONERROR;
        if (buttons == (TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON)) {
            fallback = MB_YESNOCANCEL | MB_ICONWARNING;
        }
        return ::MessageBoxW(owner, content.c_str(), windowTitle, fallback);
    }
    return button;
}

} // namespace

SaveDecision AskSaveChanges(HWND owner, const std::wstring& documentTitle)
{
    const std::wstring content = L"“" + documentTitle + L"”有未保存的更改。";

    const int button = showTaskDialog(owner, TD_WARNING_ICON, L"是否保存更改？", content,
                                      TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON,
                                      L"ActiveXWebMark");
    if (button == IDYES) {
        return SaveDecision::Save;
    }
    if (button == IDNO) {
        return SaveDecision::Discard;
    }
    return SaveDecision::Cancel;
}

void ShowError(HWND owner, const std::wstring& message)
{
    showTaskDialog(owner, TD_ERROR_ICON, L"操作失败", message, TDCBF_OK_BUTTON,
                   L"ActiveXWebMark");
}

void ShowInfo(HWND owner, const std::wstring& message)
{
    showTaskDialog(owner, TD_INFORMATION_ICON, L"ActiveXWebMark", message, TDCBF_OK_BUTTON,
                   L"ActiveXWebMark");
}

UpdatePromptChoice AskUpdatePrompt(HWND owner, const std::wstring& currentVersion,
                                   const ReleaseInfo& release, bool hasInstaller)
{
    constexpr int kInstallButton = 1001;
    constexpr int kOpenPageButton = 1002;
    constexpr int kLaterButton = 1003;
    constexpr int kIgnoreButton = 1004;

    const std::wstring version = utf8ToWide(release.version);
    const std::wstring releaseUrl = utf8ToWide(release.releaseUrl);
    const bool linkable = isGitHubHttpsUrl(releaseUrl);

    std::wstring ignoreText = L"忽略此版本\n不再提示 v" + version;

    TASKDIALOG_BUTTON buttons[3] = {};
    UINT buttonCount = 0;
    const int primaryButton = hasInstaller ? kInstallButton : kOpenPageButton;

    buttons[buttonCount].nButtonID = primaryButton;
    buttons[buttonCount].pszButtonText =
        hasInstaller ? L"立即升级\n下载安装包并更新到新版本"
                     : L"打开下载页面\n在浏览器中查看 GitHub Release";
    ++buttonCount;

    buttons[buttonCount].nButtonID = kLaterButton;
    buttons[buttonCount].pszButtonText = L"下次再说\n下次启动时再次提醒";
    ++buttonCount;

    buttons[buttonCount].nButtonID = kIgnoreButton;
    buttons[buttonCount].pszButtonText = ignoreText.c_str();
    ++buttonCount;

    const std::wstring instruction = L"发现新版本 v" + version;
    std::wstring content =
        L"当前版本：v" + currentVersion + L"\n最新版本：v" + version;
    const std::wstring date = publishedDate(release.publishedAt);
    if (!date.empty()) {
        content += L"（" + date + L" 发布）";
    }
    if (hasInstaller) {
        const ReleaseAsset* asset = findInstallerAsset(release);
        if (asset != nullptr && asset->size > 0) {
            content += L"\n安装包大小：" + formatBytes(asset->size);
        }
    } else {
        content += L"\n\n此版本没有提供安装包，请在 GitHub 发布页面手动下载。";
    }

    const std::wstring notes = releaseNotesForDisplay(release.notes, 4000);
    std::wstring footer;
    if (linkable) {
        footer = L"<a href=\"" + escapeHtmlAttribute(releaseUrl) + L"\">查看完整发布说明</a>";
    }

    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW |
                     TDF_USE_COMMAND_LINKS | TDF_SIZE_TO_CONTENT;
    config.hInstance = ::GetModuleHandleW(nullptr);
    config.pszWindowTitle = L"检查更新";
    config.pszMainIcon = MAKEINTRESOURCEW(IDI_APPICON);
    config.pszMainInstruction = instruction.c_str();
    config.pszContent = content.c_str();
    config.pButtons = buttons;
    config.cButtons = buttonCount;
    config.nDefaultButton = primaryButton;

    if (!notes.empty()) {
        config.pszExpandedInformation = notes.c_str();
        config.pszExpandedControlText = L"隐藏更新内容";
        config.pszCollapsedControlText = L"更新内容";
    }
    if (!footer.empty()) {
        config.dwFlags |= TDF_ENABLE_HYPERLINKS;
        config.pszFooter = footer.c_str();
        config.pfCallback = UpdateHyperlinkCallback;
    }

    int button = 0;
    if (FAILED(::TaskDialogIndirect(&config, &button, nullptr, nullptr))) {
        const UINT type = hasInstaller ? (MB_YESNOCANCEL | MB_ICONINFORMATION)
                                       : (MB_OKCANCEL | MB_ICONINFORMATION);
        const int fallback = ::MessageBoxW(owner, content.c_str(),
                                           L"ActiveXWebMark - 发现新版本", type);
        if (hasInstaller) {
            return fallback == IDYES ? UpdatePromptChoice::Install
                                     : UpdatePromptChoice::Later;
        }
        return fallback == IDOK ? UpdatePromptChoice::OpenPage
                                : UpdatePromptChoice::Later;
    }

    switch (button) {
    case kInstallButton:
        return UpdatePromptChoice::Install;
    case kOpenPageButton:
        return UpdatePromptChoice::OpenPage;
    case kIgnoreButton:
        return UpdatePromptChoice::Ignore;
    case kLaterButton:
    default:
        return UpdatePromptChoice::Later;
    }
}

void ShowAboutDialog(HWND owner)
{
    const std::wstring versionText = utf8ToWide(version());
#ifdef _DEBUG
    const wchar_t* build = L"Debug";
#else
    const wchar_t* build = L"Release";
#endif

    const std::wstring content =
        L"版本 " + versionText + L" (" + build + L")\n"
        L"一个原生 Win32/WTL Markdown 阅读器。\n\n"
        L"技术栈：C++17 · WTL · Scintilla · cmark · GDI+ · WinHTTP\n"
        L"预览引擎：IE/MSHTML\n\n"
        L"第三方组件：discount (BSD-3)、cmark (BSD-2)、PEG Markdown Highlight (MIT)、"
        L"Scintilla (MIT)、Prism (MIT)、WTL (MS-PL)\n"
        L"主题配色参考 MarkText (MIT)";

    constexpr const wchar_t* kProjectUrl =
        L"https://github.com/tools172/ActiveXWebMark";
    const std::wstring footer = L"<a href=\"" + std::wstring(kProjectUrl) + L"\">项目主页</a>";

    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW |
                     TDF_ENABLE_HYPERLINKS;
    config.hInstance = ::GetModuleHandleW(nullptr);
    config.pszWindowTitle = L"关于 ActiveXWebMark";
    config.pszMainIcon = MAKEINTRESOURCEW(IDI_APPICON);
    config.pszMainInstruction = L"ActiveXWebMark";
    config.pszContent = content.c_str();
    config.pszFooter = footer.c_str();
    config.pfCallback = AboutHyperlinkCallback;
    config.dwCommonButtons = TDCBF_OK_BUTTON;

    int button = 0;
    if (FAILED(::TaskDialogIndirect(&config, &button, nullptr, nullptr))) {
        MessageBoxW(owner, content.c_str(), L"关于 ActiveXWebMark",
                    MB_OK | MB_ICONINFORMATION);
    }
}

} // namespace awm
