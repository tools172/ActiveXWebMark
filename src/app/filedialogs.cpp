#include "filedialogs.h"

#include <commdlg.h>

#include <algorithm>

#include "util/pathutil.h"

namespace awm {

namespace {

constexpr const wchar_t* kFilter =
    L"Markdown 文件 (*.md;*.markdown;*.mdown)\0*.md;*.markdown;*.mdown\0"
    L"所有文件 (*.*)\0*.*\0";

constexpr const wchar_t* kThemeFilter =
    L"主题文件 (*.awmtheme.json;*.json;*.jsonc;*.tmtheme;*.vsix;*.yaml;*.yml)\0"
    L"*.awmtheme.json;*.json;*.jsonc;*.tmtheme;*.vsix;*.yaml;*.yml\0"
    L"ActiveXWebMark 主题 (*.awmtheme.json)\0*.awmtheme.json\0"
    L"VS Code 主题包 (*.vsix)\0*.vsix\0"
    L"所有文件 (*.*)\0*.*\0";

constexpr const wchar_t* kThemeSaveFilter =
    L"ActiveXWebMark 主题 (*.awmtheme.json)\0*.awmtheme.json\0"
    L"JSON 文件 (*.json)\0*.json\0";

} // namespace

std::vector<std::wstring> ShowOpenMarkdownDialog(HWND owner)
{
    constexpr DWORD kBufferChars = 64 * 1024;
    std::vector<wchar_t> buffer(kBufferChars, L'\0');

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = kFilter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = kBufferChars;
    dialog.lpstrTitle = L"打开 Markdown 文件";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT |
                   OFN_EXPLORER | OFN_NOCHANGEDIR;

    if (::GetOpenFileNameW(&dialog) == FALSE) {
        return {};
    }
    return parseMultiSelectPaths(buffer.data(), buffer.size());
}

std::wstring ShowSaveMarkdownDialog(HWND owner, const std::wstring& suggestedPath)
{
    std::vector<wchar_t> buffer(MAX_PATH * 4, L'\0');
    if (!suggestedPath.empty()) {
        const std::size_t count = std::min(suggestedPath.size(), buffer.size() - 1);
        std::copy_n(suggestedPath.begin(), count, buffer.begin());
    }

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = kFilter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = L"另存为";
    dialog.lpstrDefExt = L"md";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (::GetSaveFileNameW(&dialog) == FALSE) {
        return {};
    }
    return std::wstring(buffer.data());
}

std::vector<std::wstring> ShowOpenThemeDialog(HWND owner)
{
    constexpr DWORD kBufferChars = 64 * 1024;
    std::vector<wchar_t> buffer(kBufferChars, L'\0');

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = kThemeFilter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = kBufferChars;
    dialog.lpstrTitle = L"导入主题";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT |
                   OFN_EXPLORER | OFN_NOCHANGEDIR;

    if (::GetOpenFileNameW(&dialog) == FALSE) {
        return {};
    }
    return parseMultiSelectPaths(buffer.data(), buffer.size());
}

std::wstring ShowSaveThemeDialog(HWND owner, const std::wstring& suggestedPath)
{
    std::vector<wchar_t> buffer(MAX_PATH * 4, L'\0');
    if (!suggestedPath.empty()) {
        const std::size_t count = std::min(suggestedPath.size(), buffer.size() - 1);
        std::copy_n(suggestedPath.begin(), count, buffer.begin());
    }

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = kThemeSaveFilter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = L"导出主题";
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (::GetSaveFileNameW(&dialog) == FALSE) {
        return {};
    }
    return std::wstring(buffer.data());
}

} // namespace awm
