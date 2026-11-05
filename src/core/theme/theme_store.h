#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "theme/theme_source.h"

namespace awm::theme {

struct StoredTheme {
    std::filesystem::path path;
    ThemeSource source;
    ThemeDefinition definition;
};

struct ThemeStoreLoadResult {
    std::vector<StoredTheme> themes;
    std::vector<std::string> errors;
};

/// Loads every `*.json` theme below `directory`. Invalid files are skipped and
/// reported; the caller decides whether to surface the errors.
ThemeStoreLoadResult LoadThemeStore(const std::filesystem::path& directory);

/// Writes the theme as `<directory>/<id>.awmtheme.json`. Assigns a fresh id
/// when `source.id` is empty or would collide with a built-in theme.
bool SaveThemeToStore(const std::filesystem::path& directory, ThemeSource& source,
                      std::string& error, bool overwriteExisting = true);

/// Deletes the file whose parsed theme id matches `id`.
bool DeleteThemeFromStore(const std::filesystem::path& directory, const std::string& id,
                          std::string& error);

std::vector<ThemeDefinition> DefinitionsOf(const std::vector<StoredTheme>& themes);

/// Newline-separated ids of the compiled-in themes.
std::string BuiltinThemeIds();

std::filesystem::path ThemeFilePath(const std::filesystem::path& directory,
                                    const std::string& id);

} // namespace awm::theme
