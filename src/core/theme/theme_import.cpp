#include "theme/theme_import.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <system_error>

#include <nlohmann/json.hpp>

#include "theme/theme_import_internal.h"
#include "theme/theme_json.h"
#include "util/fileutil.h"

namespace awm::theme {

namespace {

std::string ExtensionLower(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    for (char& ch : extension) {
        ch = (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    }
    return extension;
}

bool LooksNative(const nlohmann::json& root)
{
    if (!root.is_object()) {
        return false;
    }
    if (root.contains("schemaVersion") || root.contains("markdown")) {
        return true;
    }
    const auto colors = root.find("colors");
    if (colors == root.end() || !colors->is_object()) {
        return false;
    }
    return colors->contains("background") && colors->contains("text") &&
           !colors->contains("editor.background");
}

bool LooksVscode(const nlohmann::json& root)
{
    if (!root.is_object()) {
        return false;
    }
    if (root.contains("type") || root.contains("tokenColors") || root.contains("include")) {
        return true;
    }
    const auto colors = root.find("colors");
    return colors != root.end() && colors->is_object() &&
           (colors->contains("editor.background") || colors->contains("editor.foreground"));
}

ImportResult ImportNative(const std::string& text, const std::filesystem::path& path)
{
    ThemeLoadResult loaded = LoadThemeSourceJson(text, path.filename().string());
    ImportResult result;
    if (!loaded.source) {
        result.error = loaded.error;
        return result;
    }
    ImportedTheme imported;
    imported.source = std::move(*loaded.source);
    imported.warnings = std::move(loaded.warnings);
    result.themes.push_back(std::move(imported));
    return result;
}

} // namespace

ImportResult ImportThemeFile(const std::filesystem::path& path)
{
    ImportResult result;
    const std::string extension = ExtensionLower(path);

    if (extension == ".vsix") {
        return detail::ImportVsixFile(path);
    }
    if (extension == ".tmtheme") {
        return detail::ImportTmThemeFile(path);
    }
    if (extension == ".yaml" || extension == ".yml") {
        return detail::ImportBase16File(path);
    }
    if (extension != ".json" && extension != ".jsonc" && extension != ".awmtheme") {
        result.error = "不支持的主题文件类型：" + path.extension().string();
        return result;
    }

    std::error_code ec;
    const std::optional<std::string> text = readFile(path.wstring(), ec);
    if (!text) {
        result.error = "无法读取主题文件：" + path.string() + " (" + ec.message() + ")";
        return result;
    }
    constexpr std::size_t kMaxThemeJsonBytes = 2u * 1024u * 1024u;
    if (text->size() > kMaxThemeJsonBytes) {
        result.error = "主题 JSON 超过 2 MB。";
        return result;
    }

    const std::string cleaned = StripJsonComments(*text);
    const nlohmann::json root = nlohmann::json::parse(cleaned, nullptr, false);
    if (root.is_discarded()) {
        result.error = "主题文件不是有效的 JSON。";
        return result;
    }
    if (root.is_array()) {
        result.error = "暂不支持 JSON 数组形式的主题包，请逐个导入主题文件。";
        return result;
    }

    if (LooksNative(root)) {
        return ImportNative(cleaned, path);
    }
    if (LooksVscode(root) || extension == ".jsonc") {
        return detail::ImportVscodeThemeFile(path);
    }

    // Ambiguous: try the native schema first, then the VS Code adapter.
    ImportResult native = ImportNative(cleaned, path);
    if (!native.themes.empty()) {
        return native;
    }
    ImportResult vscode = detail::ImportVscodeThemeFile(path);
    if (!vscode.themes.empty()) {
        return vscode;
    }
    result.error = native.error.empty() ? vscode.error : native.error;
    return result;
}

} // namespace awm::theme
