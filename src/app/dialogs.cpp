#include "dialogs.h"

#include <commctrl.h>
#include <shellapi.h>

#include "resource.h"
#include "util/stringutil.h"
#include "version.h"

namespace awm {

namespace {

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
        L"技术栈：C++17 · WTL · Scintilla · cmark · GDI+\n"
        L"预览引擎：IE/MSHTML\n\n"
        L"第三方组件：discount (BSD-3)、cmark (BSD-2)、PEG Markdown Highlight (MIT)、"
        L"Scintilla (MIT)、WTL (MS-PL)";

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
