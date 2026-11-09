#pragma once

#include <filesystem>

#include "theme/theme_import.h"

namespace awm::theme::detail {

ImportResult ImportVscodeThemeFile(const std::filesystem::path& path);
ImportResult ImportTmThemeFile(const std::filesystem::path& path);
ImportResult ImportBase16File(const std::filesystem::path& path);
ImportResult ImportVsixFile(const std::filesystem::path& path);

/// Loads a TextMate color scheme's token rules into an existing draft. Used by
/// the VS Code importer when `tokenColors` is a path to a `.tmTheme`.
bool LoadTmThemeTokens(const std::filesystem::path& path, TokenColors& tokens,
                       bool& any, std::vector<std::string>& warnings);

} // namespace awm::theme::detail
