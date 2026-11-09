#pragma once

#include <Windows.h>

#include <string>
#include <vector>

namespace awm {

/// Shows the multi-select Markdown open dialog. Returns an empty list on cancel.
std::vector<std::wstring> ShowOpenMarkdownDialog(HWND owner);

/// Shows the Save As dialog. Returns an empty string on cancel.
std::wstring ShowSaveMarkdownDialog(HWND owner, const std::wstring& suggestedPath);

/// Shows the multi-select theme import dialog (native JSON, VS Code JSON/VSIX,
/// tmTheme, Base16 YAML). Returns an empty list on cancel.
std::vector<std::wstring> ShowOpenThemeDialog(HWND owner);

/// Shows the theme export dialog. Returns an empty string on cancel.
std::wstring ShowSaveThemeDialog(HWND owner, const std::wstring& suggestedPath);

} // namespace awm
