#include "theme/theme_import_internal.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "util/fileutil.h"

namespace awm::theme::detail {

namespace {

struct XmlNode {
    std::string tag;
    std::string text;
    std::vector<XmlNode> children;
};

struct TokenRule {
    std::string scope;
    ThemeColor color;
};

struct TmThemeData {
    std::string name;
    ThemeColor background = Rgb(30, 30, 30);
    ThemeColor foreground = Rgb(230, 230, 230);
    ThemeColor selection = Rgba(255, 255, 255, 40);
    bool hasBackground = false;
    bool hasForeground = false;
    std::vector<TokenRule> rules;
};

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

std::string DecodeXml(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] != '&') {
            result.push_back(text[i++]);
            continue;
        }
        const std::size_t semicolon = text.find(';', i + 1);
        if (semicolon == std::string_view::npos || semicolon - i > 12) {
            result.push_back(text[i++]);
            continue;
        }
        const std::string_view entity = text.substr(i + 1, semicolon - i - 1);
        if (entity == "amp") {
            result.push_back('&');
        } else if (entity == "lt") {
            result.push_back('<');
        } else if (entity == "gt") {
            result.push_back('>');
        } else if (entity == "quot") {
            result.push_back('"');
        } else if (entity == "apos") {
            result.push_back('\'');
        } else if (entity.size() > 1 && entity[0] == '#') {
            int value = 0;
            bool valid = false;
            if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
                for (std::size_t k = 2; k < entity.size(); ++k) {
                    const char ch = entity[k];
                    int digit = -1;
                    if (ch >= '0' && ch <= '9') {
                        digit = ch - '0';
                    } else if (ch >= 'a' && ch <= 'f') {
                        digit = ch - 'a' + 10;
                    } else if (ch >= 'A' && ch <= 'F') {
                        digit = ch - 'A' + 10;
                    }
                    if (digit < 0) {
                        valid = false;
                        break;
                    }
                    value = value * 16 + digit;
                    valid = true;
                }
            } else {
                for (std::size_t k = 1; k < entity.size(); ++k) {
                    const char ch = entity[k];
                    if (ch < '0' || ch > '9') {
                        valid = false;
                        break;
                    }
                    value = value * 10 + (ch - '0');
                    valid = true;
                }
            }
            if (valid && value > 0 && value < 0x80) {
                result.push_back(static_cast<char>(value));
            } else {
                result += std::string(text.substr(i, semicolon - i + 1));
            }
        } else {
            result += std::string(text.substr(i, semicolon - i + 1));
        }
        i = semicolon + 1;
    }
    return result;
}

bool SkipMisc(const std::string& input, std::size_t& pos, std::string& error)
{
    while (pos < input.size()) {
        if (std::isspace(static_cast<unsigned char>(input[pos])) != 0) {
            ++pos;
            continue;
        }
        if (input.compare(pos, 4, "<!--") == 0) {
            const std::size_t end = input.find("-->", pos + 4);
            if (end == std::string::npos) {
                error = "XML 注释未闭合";
                return false;
            }
            pos = end + 3;
            continue;
        }
        if (input.compare(pos, 2, "<?") == 0) {
            const std::size_t end = input.find("?>", pos + 2);
            if (end == std::string::npos) {
                error = "XML 处理指令未闭合";
                return false;
            }
            pos = end + 2;
            continue;
        }
        if (input.compare(pos, 9, "<!DOCTYPE") == 0) {
            const std::size_t end = input.find('>', pos + 9);
            if (end == std::string::npos) {
                error = "XML DOCTYPE 未闭合";
                return false;
            }
            pos = end + 1;
            continue;
        }
        if (input.compare(pos, 9, "<!ENTITY") == 0) {
            error = "不支持 XML 实体声明";
            return false;
        }
        break;
    }
    return true;
}

bool ParseNode(const std::string& input, std::size_t& pos, XmlNode& node, int depth,
               std::string& error)
{
    if (depth > 32) {
        error = "XML 嵌套层级过深";
        return false;
    }
    if (!SkipMisc(input, pos, error)) {
        return false;
    }
    if (pos >= input.size() || input[pos] != '<') {
        error = "XML 结构无效";
        return false;
    }
    ++pos;
    const std::size_t nameStart = pos;
    while (pos < input.size() &&
           std::isspace(static_cast<unsigned char>(input[pos])) == 0 &&
           input[pos] != '>' && input[pos] != '/') {
        ++pos;
    }
    node.tag = input.substr(nameStart, pos - nameStart);
    if (node.tag.empty()) {
        error = "XML 标签名为空";
        return false;
    }

    bool selfClosing = false;
    while (pos < input.size()) {
        const char ch = input[pos];
        if (ch == '"' || ch == '\'') {
            const char quote = ch;
            ++pos;
            while (pos < input.size() && input[pos] != quote) {
                ++pos;
            }
            if (pos < input.size()) {
                ++pos;
            }
            continue;
        }
        if (ch == '/' && pos + 1 < input.size() && input[pos + 1] == '>') {
            selfClosing = true;
            pos += 2;
            break;
        }
        if (ch == '>') {
            ++pos;
            break;
        }
        ++pos;
    }
    if (selfClosing) {
        return true;
    }

    while (pos < input.size()) {
        if (input.compare(pos, 2, "</") == 0) {
            pos += 2;
            const std::size_t closeStart = pos;
            while (pos < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[pos])) == 0 &&
                   input[pos] != '>') {
                ++pos;
            }
            const std::string closing = input.substr(closeStart, pos - closeStart);
            if (closing != node.tag) {
                error = "XML 标签不匹配：<" + node.tag + "> / </" + closing + ">";
                return false;
            }
            const std::size_t end = input.find('>', pos);
            if (end == std::string::npos) {
                error = "XML 结束标签未闭合";
                return false;
            }
            pos = end + 1;
            return true;
        }
        if (input[pos] == '<') {
            XmlNode child;
            if (!ParseNode(input, pos, child, depth + 1, error)) {
                return false;
            }
            node.children.push_back(std::move(child));
            continue;
        }
        const std::size_t next = input.find('<', pos);
        const std::string_view text =
            std::string_view(input).substr(pos, next == std::string::npos
                                                    ? std::string_view::npos
                                                    : next - pos);
        node.text += DecodeXml(text);
        pos = next == std::string::npos ? input.size() : next;
    }

    error = "XML 标签未闭合：<" + node.tag + ">";
    return false;
}

const XmlNode* FindChild(const XmlNode& node, std::string_view tag)
{
    for (const XmlNode& child : node.children) {
        if (child.tag == tag) {
            return &child;
        }
    }
    return nullptr;
}

void ForEachDictEntry(const XmlNode& dict,
                      const std::function<void(const std::string&, const XmlNode&)>& fn)
{
    for (std::size_t i = 0; i + 1 < dict.children.size(); ++i) {
        if (dict.children[i].tag != "key") {
            continue;
        }
        fn(dict.children[i].text, dict.children[i + 1]);
        ++i;
    }
}

std::optional<ThemeColor> StringColor(const XmlNode& node)
{
    if (node.tag != "string") {
        return std::nullopt;
    }
    return ParseColor(node.text);
}

void AddScopes(const std::string& scopeText, const ThemeColor& color, TmThemeData& data)
{
    std::size_t start = 0;
    while (start <= scopeText.size()) {
        const std::size_t comma = scopeText.find(',', start);
        const std::string_view scope = Trim(std::string_view(scopeText).substr(
            start, comma == std::string::npos ? std::string_view::npos : comma - start));
        if (!scope.empty() && scope.front() != '-') {
            data.rules.push_back(TokenRule{std::string(scope), color});
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
}

bool ParseTmTheme(const std::filesystem::path& path, TmThemeData& data, std::string& error)
{
    std::error_code ec;
    const std::optional<std::string> text = readFile(path.wstring(), ec);
    if (!text) {
        error = "无法读取 tmTheme：" + path.string() + " (" + ec.message() + ")";
        return false;
    }
    constexpr std::size_t kMaxBytes = 2 * 1024 * 1024;
    if (text->size() > kMaxBytes) {
        error = "tmTheme 文件超过 2 MB";
        return false;
    }
    if (text->find("<!ENTITY") != std::string::npos) {
        error = "tmTheme 包含不支持的实体声明";
        return false;
    }

    std::size_t pos = 0;
    XmlNode root;
    if (!ParseNode(*text, pos, root, 0, error)) {
        return false;
    }
    const XmlNode* plist = root.tag == "plist" ? &root : FindChild(root, "plist");
    if (plist == nullptr) {
        error = "tmTheme 缺少 plist 根节点";
        return false;
    }
    const XmlNode* dict = FindChild(*plist, "dict");
    if (dict == nullptr) {
        error = "tmTheme 缺少 dict 根节点";
        return false;
    }

    const XmlNode* settings = nullptr;
    ForEachDictEntry(*dict, [&](const std::string& key, const XmlNode& value) {
        if (key == "name" && value.tag == "string") {
            data.name = value.text;
        } else if (key == "settings" && value.tag == "array") {
            settings = &value;
        }
    });
    if (settings == nullptr) {
        error = "tmTheme 缺少 settings 数组";
        return false;
    }

    for (const XmlNode& entry : settings->children) {
        if (entry.tag != "dict") {
            continue;
        }
        const XmlNode* ruleSettings = nullptr;
        std::vector<std::string> scopes;
        ForEachDictEntry(entry, [&](const std::string& key, const XmlNode& value) {
            if (key == "settings" && value.tag == "dict") {
                ruleSettings = &value;
            } else if (key == "scope" && value.tag == "string") {
                scopes.push_back(value.text);
            } else if (key == "scope" && value.tag == "array") {
                for (const XmlNode& item : value.children) {
                    if (item.tag == "string") {
                        scopes.push_back(item.text);
                    }
                }
            }
        });
        if (ruleSettings == nullptr) {
            continue;
        }

        std::optional<ThemeColor> foreground;
        std::optional<ThemeColor> background;
        std::optional<ThemeColor> selection;
        ForEachDictEntry(*ruleSettings, [&](const std::string& key, const XmlNode& value) {
            if (key == "foreground") {
                foreground = StringColor(value);
            } else if (key == "background") {
                background = StringColor(value);
            } else if (key == "selection") {
                selection = StringColor(value);
            }
        });

        if (scopes.empty()) {
            if (background) {
                data.background = *background;
                data.hasBackground = true;
            }
            if (foreground) {
                data.foreground = *foreground;
                data.hasForeground = true;
            }
            if (selection) {
                data.selection = *selection;
            }
            continue;
        }
        if (foreground) {
            for (const std::string& scope : scopes) {
                AddScopes(scope, *foreground, data);
            }
        }
        if (data.rules.size() > 5000) {
            error = "tmTheme 规则过多";
            return false;
        }
    }
    return true;
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

constexpr const char* kBucketSelectors[] = {
    "comment",        "punctuation",       "entity.name.tag", "variable.other",
    "constant.numeric", "string",          "keyword.operator", "entity.name.function",
    "keyword",        "string.regexp",
};

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

TokenColors TokensFromData(const TmThemeData& data)
{
    TokenColors tokens{};
    const ThemeColor fg = data.foreground;
    const ThemeColor bg = data.background;
    tokens.comment = Blend(fg, bg, 0.55);
    tokens.punctuation = Blend(fg, bg, 0.10);
    tokens.tag = Blend(fg, bg, 0.05);
    tokens.property = Blend(fg, bg, 0.15);
    tokens.booleanNumber = Blend(fg, bg, 0.25);
    tokens.string = Blend(fg, bg, 0.35);
    tokens.oper = Blend(fg, bg, 0.20);
    tokens.function = Blend(fg, bg, 0.10);
    tokens.keyword = Blend(fg, bg, 0.05);
    tokens.regex = Blend(fg, bg, 0.30);

    int scores[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    for (const TokenRule& rule : data.rules) {
        for (int i = 0; i < 10; ++i) {
            const int score = ScopeScore(rule.scope, kBucketSelectors[i]);
            if (score > 0 && score >= scores[i]) {
                scores[i] = score;
                *TokenAt(tokens, i) = rule.color;
            }
        }
    }
    return tokens;
}

} // namespace

bool LoadTmThemeTokens(const std::filesystem::path& path, TokenColors& tokens, bool& any,
                       std::vector<std::string>& warnings)
{
    TmThemeData data;
    std::string error;
    if (!ParseTmTheme(path, data, error)) {
        warnings.push_back(error);
        return false;
    }
    tokens = TokensFromData(data);
    any = true;
    return true;
}

ImportResult ImportTmThemeFile(const std::filesystem::path& path)
{
    ImportResult result;
    TmThemeData data;
    std::string error;
    if (!ParseTmTheme(path, data, error)) {
        result.error = error;
        return result;
    }

    ThemeSourceDraft draft;
    draft.name = data.name.empty() ? path.stem().string() : data.name;
    draft.source = "TextMate theme: " + path.filename().string();
    draft.background = data.background;
    draft.text = data.foreground;
    draft.selection = data.selection;
    draft.findMatch = Rgba(data.selection.r, data.selection.g, data.selection.b, 102);
    draft.findCurrent = data.selection;
    draft.findText = data.foreground;
    draft.tokens = TokensFromData(data);

    ImportedTheme imported;
    imported.source = FinalizeSource(std::move(draft));
    result.themes.push_back(std::move(imported));
    return result;
}

} // namespace awm::theme::detail
