#include "theme/theme_import_internal.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "theme/theme_json.h"
#include "util/fileutil.h"

namespace awm::theme::detail {

namespace {

using Json = nlohmann::json;

struct TokenBucket {
    const char* selector;
    const char* name;
};

constexpr TokenBucket kBuckets[] = {
    {"comment", "comment"},
    {"punctuation", "punctuation"},
    {"entity.name.tag", "tag"},
    {"variable.other", "property"},
    {"constant.numeric", "booleanNumber"},
    {"string", "string"},
    {"keyword.operator", "operator"},
    {"entity.name.function", "function"},
    {"keyword", "keyword"},
    {"string.regexp", "regex"},
};

constexpr int kBucketCount = 10;

char AsciiLower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

std::string LowerAscii(std::string text)
{
    for (char& ch : text) {
        ch = AsciiLower(ch);
    }
    return text;
}

std::string_view Trim(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

ThemeColor* TokenAt(TokenColors& tokens, int index)
{
    switch (index) {
    case 0:
        return &tokens.comment;
    case 1:
        return &tokens.punctuation;
    case 2:
        return &tokens.tag;
    case 3:
        return &tokens.property;
    case 4:
        return &tokens.booleanNumber;
    case 5:
        return &tokens.string;
    case 6:
        return &tokens.oper;
    case 7:
        return &tokens.function;
    case 8:
        return &tokens.keyword;
    case 9:
        return &tokens.regex;
    default:
        return nullptr;
    }
}

std::optional<ThemeColor> ColorFromJson(const Json& value)
{
    if (value.is_string()) {
        return ParseColor(value.get<std::string>());
    }
    if (value.is_object()) {
        const auto foreground = value.find("foreground");
        if (foreground != value.end() && foreground->is_string()) {
            return ParseColor(foreground->get<std::string>());
        }
    }
    return std::nullopt;
}

std::optional<ThemeColor> PickColor(const Json& colors,
                                    std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const auto it = colors.find(key);
        if (it == colors.end()) {
            continue;
        }
        if (const auto color = ColorFromJson(*it)) {
            return color;
        }
    }
    return std::nullopt;
}

int SegmentCount(std::string_view selector)
{
    int count = 1;
    for (const char ch : selector) {
        if (ch == '.') {
            ++count;
        }
    }
    return count;
}

/// TextMate selectors are hierarchical. We use the last space-separated part
/// (the most concrete scope) and compare it with the bucket's representative
/// scope. A longer, more specific selector wins; later rules win ties.
int ScopeScore(std::string_view selector, std::string_view target)
{
    selector = Trim(selector);
    if (selector.empty() || selector.front() == '-' || selector == "*") {
        return 0;
    }
    const std::size_t space = selector.rfind(' ');
    if (space != std::string_view::npos) {
        selector = Trim(selector.substr(space + 1));
    }
    if (selector.empty() || selector.find('*') != std::string_view::npos) {
        return 0;
    }

    const bool matches = selector == target ||
                         (target.size() > selector.size() &&
                          target.compare(0, selector.size(), selector) == 0 &&
                          target[selector.size()] == '.') ||
                         (selector.size() > target.size() &&
                          selector.compare(0, target.size(), target) == 0 &&
                          selector[target.size()] == '.');
    if (!matches) {
        return 0;
    }
    return SegmentCount(selector) * 100 + static_cast<int>(selector.size());
}

struct VscodeContext {
    ThemeSourceDraft draft;
    std::vector<std::string> warnings;
    TokenColors tokens{};
    bool tokenPresent[kBucketCount] = {};
    int tokenScore[kBucketCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    bool anyToken = false;
    std::set<std::string> visited;
};

void ApplyTokenScope(VscodeContext& context, std::string_view rawScope,
                     const ThemeColor& color)
{
    // Comma lists are alternatives.
    std::size_t start = 0;
    while (start <= rawScope.size()) {
        const std::size_t comma = rawScope.find(',', start);
        const std::string_view selector =
            Trim(rawScope.substr(start, comma == std::string_view::npos
                                            ? std::string_view::npos
                                            : comma - start));
        if (!selector.empty()) {
            for (int i = 0; i < kBucketCount; ++i) {
                const int score = ScopeScore(selector, kBuckets[i].selector);
                if (score > 0 && score >= context.tokenScore[i]) {
                    context.tokenScore[i] = score;
                    *TokenAt(context.tokens, i) = color;
                    context.tokenPresent[i] = true;
                    context.anyToken = true;
                }
            }
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
}

void ApplyTokenArray(const Json& rules, VscodeContext& context)
{
    if (!rules.is_array()) {
        return;
    }
    for (const auto& rule : rules) {
        if (!rule.is_object()) {
            continue;
        }
        const auto settings = rule.find("settings");
        if (settings == rule.end() || !settings->is_object()) {
            continue;
        }
        const auto color = ColorFromJson(*settings);
        if (!color) {
            continue;
        }
        const auto scope = rule.find("scope");
        if (scope == rule.end()) {
            continue;
        }
        if (scope->is_string()) {
            ApplyTokenScope(context, scope->get<std::string>(), *color);
        } else if (scope->is_array()) {
            for (const auto& entry : *scope) {
                if (entry.is_string()) {
                    ApplyTokenScope(context, entry.get<std::string>(), *color);
                }
            }
        }
    }
}

void ApplySemanticTokens(const Json& semantic, VscodeContext& context)
{
    if (!semantic.is_object()) {
        return;
    }
    struct Mapping {
        const char* key;
        int bucket;
    };
    static const Mapping kMappings[] = {
        {"comment", 0},  {"punctuation", 1}, {"type", 2},     {"variable", 3},
        {"number", 4},   {"string", 5},      {"operator", 6}, {"function", 7},
        {"keyword", 8},  {"regexp", 9},
    };
    for (const auto& [key, value] : semantic.items()) {
        const std::string lower = LowerAscii(key);
        int bucket = -1;
        for (const Mapping& mapping : kMappings) {
            if (lower == mapping.key) {
                bucket = mapping.bucket;
                break;
            }
        }
        if (bucket < 0) {
            continue;
        }
        if (const auto color = ColorFromJson(value)) {
            *TokenAt(context.tokens, bucket) = *color;
            context.tokenPresent[bucket] = true;
            context.anyToken = true;
        }
    }
}

void ApplyColors(const Json& colors, VscodeContext& context)
{
    if (!colors.is_object()) {
        return;
    }
    ThemeSourceDraft& draft = context.draft;
    const auto assign = [&colors, &draft](std::initializer_list<const char*> keys,
                                          std::optional<ThemeColor>& target) {
        if (const auto color = PickColor(colors, keys)) {
            target = *color;
        }
    };

    assign({"editor.background"}, draft.background);
    assign({"editor.foreground"}, draft.text);
    assign({"descriptionForeground", "disabledForeground", "editorLineNumber.foreground"},
           draft.muted);
    assign({"textLink.foreground", "textLink.activeForeground",
            "editorLink.activeForeground"},
           draft.link);
    assign({"focusBorder", "activityBarBadge.background", "button.background",
            "progressBar.background", "textLink.foreground"},
           draft.accent);
    assign({"errorForeground", "editorError.foreground",
            "notificationsErrorIcon.foreground"},
           draft.danger);
    assign({"textCodeBlock.background", "editorWidget.background",
            "editorHoverWidget.background"},
           draft.codeBackground);
    assign({"editorWidget.border", "panel.border", "editorGroup.border", "sideBar.border",
            "contrastBorder"},
           draft.border);
    assign({"sideBar.background", "editorGroupHeader.tabsBackground",
            "titleBar.activeBackground"},
           draft.sidebarBackground);
    assign({"list.hoverBackground", "sideBarSectionHeader.background",
            "tab.hoverBackground"},
           draft.sidebarHover);
    assign({"editor.selectionBackground"}, draft.selection);
    assign({"editor.findMatchHighlightBackground", "editor.findMatchBackground"},
           draft.findMatch);
    assign({"editor.findMatchBackground"}, draft.findCurrent);
    assign({"editor.findMatchForeground", "editor.foreground"}, draft.findText);

    if (draft.border) {
        draft.hr = draft.border;
        draft.tableBorder = draft.border;
        draft.codeBorder = draft.border;
    }
    if (draft.text) {
        draft.strong = draft.text;
        draft.emphasis = draft.text;
        draft.codeText = draft.text;
        for (auto& heading : draft.headings) {
            if (!heading) {
                heading = draft.text;
            }
        }
        if (!draft.listMarker) {
            draft.listMarker = draft.muted.value_or(*draft.text);
        }
        if (!draft.findText) {
            draft.findText = draft.text;
        }
    }
}

std::filesystem::path ResolveRelative(const std::filesystem::path& base,
                                      const std::string& relative)
{
    std::filesystem::path value(relative);
    if (value.is_absolute()) {
        return value;
    }
    std::filesystem::path resolved = base.parent_path() / value;
    std::error_code ec;
    if (std::filesystem::exists(resolved, ec)) {
        return resolved;
    }
    if (!resolved.has_extension()) {
        for (const char* extension : {".json", ".jsonc", ".tmTheme", ".tmtheme"}) {
            std::filesystem::path candidate = resolved;
            candidate += extension;
            if (std::filesystem::exists(candidate, ec)) {
                return candidate;
            }
        }
    }
    return resolved;
}

bool LoadRecursive(const std::filesystem::path& path, int depth, VscodeContext& context,
                   std::string& error)
{
    if (depth > 8) {
        context.warnings.push_back("include 层级超过 8 层，已停止。");
        return true;
    }

    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical = path;
    }
    if (!context.visited.insert(canonical.string()).second) {
        context.warnings.push_back("检测到 include 循环：" + path.filename().string());
        return true;
    }

    const std::optional<std::string> text = readFile(path.wstring(), ec);
    if (!text) {
        error = "无法读取 " + path.string() + " (" + ec.message() + ")";
        return false;
    }
    constexpr std::size_t kMaxThemeJsonBytes = 2u * 1024u * 1024u;
    if (text->size() > kMaxThemeJsonBytes) {
        error = path.filename().string() + " 超过 2 MB";
        return false;
    }
    const Json root = Json::parse(StripJsonComments(*text), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        error = path.filename().string() + " 不是有效的 VS Code 主题 JSON";
        return false;
    }

    if (const auto include = root.find("include");
        include != root.end() && include->is_string()) {
        const std::filesystem::path included =
            ResolveRelative(path, include->get<std::string>());
        std::string includeError;
        if (!LoadRecursive(included, depth + 1, context, includeError)) {
            context.warnings.push_back("include 加载失败：" + includeError);
        }
    }

    if (root.contains("name") && root["name"].is_string()) {
        context.draft.name = root["name"].get<std::string>();
    } else if (context.draft.name.empty()) {
        context.draft.name = path.stem().string();
    }
    if (root.contains("author") && root["author"].is_string()) {
        context.draft.author = root["author"].get<std::string>();
    }
    if (root.contains("type") && root["type"].is_string()) {
        const std::string type = LowerAscii(root["type"].get<std::string>());
        context.draft.dark = (type == "dark" || type == "hc-black");
    }

    if (const auto colors = root.find("colors"); colors != root.end()) {
        ApplyColors(*colors, context);
    }

    if (const auto tokenColors = root.find("tokenColors");
        tokenColors != root.end()) {
        if (tokenColors->is_array()) {
            ApplyTokenArray(*tokenColors, context);
        } else if (tokenColors->is_string()) {
            const std::filesystem::path tokenPath =
                ResolveRelative(path, tokenColors->get<std::string>());
            bool any = context.anyToken;
            if (LoadTmThemeTokens(tokenPath, context.tokens, any, context.warnings)) {
                context.anyToken = any;
                for (int i = 0; i < kBucketCount; ++i) {
                    context.tokenPresent[i] = true;
                }
            } else {
                context.warnings.push_back("无法读取 tokenColors 路径：" +
                                           tokenPath.string());
            }
        }
    }

    if (const auto semantic = root.find("semanticTokenColors");
        semantic != root.end()) {
        ApplySemanticTokens(*semantic, context);
    }

    return true;
}

} // namespace

ImportResult ImportVscodeThemeFile(const std::filesystem::path& path)
{
    ImportResult result;
    VscodeContext context;
    std::string error;
    if (!LoadRecursive(path, 0, context, error)) {
        result.error = error;
        return result;
    }

    if (!context.draft.background || !context.draft.text) {
        context.warnings.push_back(
            "主题没有同时提供 editor.background / editor.foreground，已使用默认值补全。");
    }

    if (context.anyToken) {
        ThemeSourceDraft baseDraft = context.draft;
        baseDraft.tokens.reset();
        const ThemeSource derived = FinalizeSource(std::move(baseDraft));
        TokenColors merged = derived.tokens;
        for (int i = 0; i < kBucketCount; ++i) {
            if (context.tokenPresent[i]) {
                *TokenAt(merged, i) = *TokenAt(context.tokens, i);
            }
        }
        context.draft.tokens = merged;
    }

    context.draft.source = "VS Code theme: " + path.filename().string();
    if (context.draft.name.empty()) {
        context.draft.name = path.stem().string();
    }

    ImportedTheme imported;
    imported.source = FinalizeSource(std::move(context.draft));
    imported.warnings = std::move(context.warnings);
    result.themes.push_back(std::move(imported));
    return result;
}

} // namespace awm::theme::detail
