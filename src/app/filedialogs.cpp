#include "filedialogs.h"

#include <commdlg.h>

#include <algorithm>

#include "util/pathutil.h"

namespace awm {

namespace {

constexpr const wchar_t* kFilter =
    L"Markdown 文件 (*.md;*.markdown;*.mdown)\0*.md;*.markdown;*.mdown\0"
    L"所有文件 (*.*)\0*.*\0";

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

} // namespace awm
