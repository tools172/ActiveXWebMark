#include "theme/theme_store.h"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "theme/theme_json.h"

namespace awm::theme {

namespace {

char AsciiLower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool AsciiLess(std::string_view a, std::string_view b)
{
    const std::size_t count = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < count; ++i) {
        const char left = AsciiLower(a[i]);
        const char right = AsciiLower(b[i]);
        if (left != right) {
            return left < right;
        }
    }
    return a.size() < b.size();
}

bool IdCollides(std::string_view id, std::string_view existingIds)
{
    std::size_t start = 0;
    while (start <= existingIds.size()) {
        const std::size_t end = existingIds.find('\n', start);
        const std::string_view line = existingIds.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (line == id) {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

std::string SanitizeId(std::string_view id)
{
    std::string result;
    result.reserve(id.size());
    for (const unsigned char raw : id) {
        const char ch = static_cast<char>(raw);
        const bool accepted = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                              (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' ||
                              ch == '.';
        result.push_back(accepted ? ch : '_');
    }
    if (result.empty() || result == "." || result == "..") {
        return "theme";
    }
    return result;
}

} // namespace

std::string BuiltinThemeIds()
{
    std::string ids;
    const std::vector<ThemeDefinition>& themes = AllThemes();
    const std::size_t count = std::min(BuiltinThemeCount(), themes.size());
    for (std::size_t i = 0; i < count; ++i) {
        ids += themes[i].id;
        ids += '\n';
    }
    return ids;
}

std::filesystem::path ThemeFilePath(const std::filesystem::path& directory,
                                    const std::string& id)
{
    return directory / (SanitizeId(id) + ".awmtheme.json");
}

ThemeStoreLoadResult LoadThemeStore(const std::filesystem::path& directory)
{
    ThemeStoreLoadResult result;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        return result;
    }

    std::string existingIds = BuiltinThemeIds();

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            result.errors.push_back("读取主题目录失败：" + ec.message());
            break;
        }
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.extension() != L".json") {
            continue;
        }
        if (result.themes.size() >= kMaxUserThemes) {
            result.errors.push_back("用户主题数量超过上限，剩余文件已跳过。");
            break;
        }

        ThemeLoadResult loaded = LoadThemeSourceFile(path);
        if (!loaded.source) {
            result.errors.push_back(path.filename().string() + ": " + loaded.error);
            continue;
        }

        ThemeSource source = std::move(*loaded.source);
        if (source.name.empty()) {
            source.name = path.stem().string();
        }
        if (source.author.empty()) {
            source.author = "本地导入";
        }
        if (source.id.empty()) {
            source.id = MakeThemeId(source.name, existingIds);
        } else if (IdCollides(source.id, existingIds)) {
            result.errors.push_back(path.filename().string() + ": id “" + source.id +
                                    "”与已有主题冲突，已跳过。");
            continue;
        }
        existingIds += source.id;
        existingIds += '\n';

        StoredTheme stored;
        stored.path = path;
        stored.definition = BuildDefinition(source, true);
        stored.source = std::move(source);
        result.themes.push_back(std::move(stored));
    }

    std::sort(result.themes.begin(), result.themes.end(),
              [](const StoredTheme& a, const StoredTheme& b) {
                  return AsciiLess(a.definition.displayName, b.definition.displayName);
              });
    return result;
}

bool SaveThemeToStore(const std::filesystem::path& directory, ThemeSource& source,
                      std::string& error, bool overwriteExisting)
{
    if (source.name.empty()) {
        error = "主题名称不能为空。";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        error = "无法创建主题目录：" + ec.message();
        return false;
    }

    const std::string builtinIds = BuiltinThemeIds();
    std::string existingIds = builtinIds;
    const ThemeStoreLoadResult existing = LoadThemeStore(directory);
    for (const StoredTheme& theme : existing.themes) {
        existingIds += theme.source.id;
        existingIds += '\n';
    }

    // Built-in ids are always reserved. A user-theme id collision is only a
    // problem when importing; editing an existing theme must keep its id.
    bool needsNewId = source.id.empty() || IdCollides(source.id, builtinIds);
    if (!needsNewId && !overwriteExisting && IdCollides(source.id, existingIds)) {
        needsNewId = true;
    }
    if (needsNewId) {
        std::string candidate = MakeThemeId(source.name, existingIds);
        int suffix = 2;
        while (IdCollides(candidate, existingIds) && suffix < 10000) {
            candidate =
                MakeThemeId(source.name + " " + std::to_string(suffix), existingIds);
            ++suffix;
        }
        source.id = candidate;
    }

    const std::filesystem::path path = ThemeFilePath(directory, source.id);
    if (!SaveThemeSourceFile(path, source, error)) {
        error = "保存主题失败：" + error;
        return false;
    }
    return true;
}

bool DeleteThemeFromStore(const std::filesystem::path& directory, const std::string& id,
                          std::string& error)
{
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        error = "主题目录不存在。";
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            error = "读取主题目录失败：" + ec.message();
            return false;
        }
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.extension() != L".json") {
            continue;
        }
        ThemeLoadResult loaded = LoadThemeSourceFile(path);
        if (!loaded.source) {
            continue;
        }
        if (loaded.source->id == id) {
            if (!std::filesystem::remove(path, ec) || ec) {
                error = "删除主题文件失败：" + ec.message();
                return false;
            }
            return true;
        }
    }
    error = "未找到该主题文件。";
    return false;
}

std::vector<ThemeDefinition> DefinitionsOf(const std::vector<StoredTheme>& themes)
{
    std::vector<ThemeDefinition> definitions;
    definitions.reserve(themes.size());
    for (const StoredTheme& theme : themes) {
        definitions.push_back(theme.definition);
    }
    return definitions;
}

} // namespace awm::theme
