#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "theme/theme_source.h"

namespace awm::theme {

struct ThemeLoadResult {
    std::optional<ThemeSource> source;
    std::vector<std::string> warnings;
    std::string error;
};

/// Parses the native Markdown-first theme JSON (a "markdown" or legacy
/// "colors" object, optional "tokens"/"typography"/"ui"/"overrides").
ThemeLoadResult LoadThemeSourceJson(const std::string& utf8,
                                    const std::string& sourceName = {});

ThemeLoadResult LoadThemeSourceFile(const std::filesystem::path& path);

/// Serializes a theme to the canonical native JSON (UTF-8, indented).
std::string ThemeSourceToJson(const ThemeSource& source);

bool SaveThemeSourceFile(const std::filesystem::path& path, const ThemeSource& source,
                         std::string& error);

/// Removes // and /* */ comments and trailing commas while preserving string
/// contents. Used for the native format and VS Code JSONC themes.
std::string StripJsonComments(const std::string& text);

} // namespace awm::theme
