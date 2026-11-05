#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "theme/theme_source.h"

namespace awm::theme {

struct ImportedTheme {
    ThemeSource source;
    std::vector<std::string> warnings;
};

struct ImportResult {
    std::vector<ImportedTheme> themes;
    std::vector<std::string> warnings;
    std::string error;
};

/// Imports a native theme, a VS Code color theme (JSON/JSONC, including
/// `include` chains), a TextMate `.tmTheme`, or a Base16/Base24 YAML scheme.
/// Everything is normalized into ThemeSource; unsupported parts are reported
/// through warnings instead of being injected as raw CSS.
ImportResult ImportThemeFile(const std::filesystem::path& path);

} // namespace awm::theme
