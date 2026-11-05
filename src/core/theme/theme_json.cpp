#include "theme/theme_json.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "util/fileutil.h"

namespace awm::theme {

namespace {

using Json = nlohmann::json;

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

std::string JoinContext(const std::string& sourceName, const std::string& message)
{
    return sourceName.empty() ? message : sourceName + ": " + message;
}

std::optional<ThemeColor> JsonColor(const Json& object, const char* key,
                                    std::vector<std::string>& warnings)
{
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    if (!it->is_string()) {
        warnings.push_back(std::string("字段 ") + key + " 不是字符串，已忽略。");
        return std::nullopt;
    }
    const std::string text = it->get<std::string>();
    const std::optional<ThemeColor> color = ParseColor(text);
    if (!color) {
        warnings.push_back(std::string("字段 ") + key + " 的颜色值无效： " + text);
    }
    return color;
}

std::optional<ThemeFamily> FamilyFromString(const std::string& text)
{
    std::string key;
    for (const char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            key.push_back(AsciiLower(ch));
        }
    }
    if (key == "neutral") {
        return ThemeFamily::Neutral;
    }
    if (key == "blue") {
        return ThemeFamily::Blue;
    }
    if (key == "cyan") {
        return ThemeFamily::Cyan;
    }
    if (key == "green") {
        return ThemeFamily::Green;
    }
    if (key == "warm") {
        return ThemeFamily::Warm;
    }
    if (key == "redpink") {
        return ThemeFamily::RedPink;
    }
    if (key == "purple") {
        return ThemeFamily::Purple;
    }
    if (key == "multicolor") {
        return ThemeFamily::Multicolor;
    }
    return std::nullopt;
}

const char* FamilyToString(ThemeFamily family)
{
    switch (family) {
    case ThemeFamily::Neutral:
        return "Neutral";
    case ThemeFamily::Blue:
        return "Blue";
    case ThemeFamily::Cyan:
        return "Cyan";
    case ThemeFamily::Green:
        return "Green";
    case ThemeFamily::Warm:
        return "Warm";
    case ThemeFamily::RedPink:
        return "RedPink";
    case ThemeFamily::Purple:
        return "Purple";
    case ThemeFamily::Multicolor:
        return "Multicolor";
    }
    return "Neutral";
}

void ReadTypography(const Json& root, ThemeTypography& typography,
                    std::vector<std::string>& warnings)
{
    const auto it = root.find("typography");
    if (it == root.end() || !it->is_object()) {
        return;
    }
    const Json& type = *it;
    if (type.contains("bodyFont") && type["bodyFont"].is_string()) {
        typography.bodyFont = type["bodyFont"].get<std::string>();
    } else if (type.contains("fontFamily") && type["fontFamily"].is_string()) {
        typography.bodyFont = type["fontFamily"].get<std::string>();
    }
    if (type.contains("codeFont") && type["codeFont"].is_string()) {
        typography.codeFont = type["codeFont"].get<std::string>();
    } else if (type.contains("monoFontFamily") && type["monoFontFamily"].is_string()) {
        typography.codeFont = type["monoFontFamily"].get<std::string>();
    }
    const auto readInt = [&type, &warnings](const char* key, int& value) {
        const auto field = type.find(key);
        if (field != type.end()) {
            if (field->is_number_integer()) {
                value = field->get<int>();
            } else {
                warnings.push_back(std::string("typography.") + key + " 不是整数，已忽略。");
            }
        }
    };
    readInt("bodySize", typography.bodySize);
    readInt("fontSize", typography.bodySize);
    readInt("maxWidth", typography.maxWidth);
    readInt("contentWidth", typography.maxWidth);
    readInt("codeSize", typography.codeSize);
    if (type.contains("lineHeight") && type["lineHeight"].is_number()) {
        typography.lineHeight = type["lineHeight"].get<double>();
    }
    if (type.contains("padding") && type["padding"].is_array()) {
        const Json& padding = type["padding"];
        if (padding.size() >= 1 && padding[0].is_number_integer()) {
            typography.padTop = padding[0].get<int>();
        }
        if (padding.size() >= 2 && padding[1].is_number_integer()) {
            typography.padRight = padding[1].get<int>();
            typography.padBottom = padding[1].get<int>();
        }
        if (padding.size() >= 3 && padding[2].is_number_integer()) {
            typography.padBottom = padding[2].get<int>();
        }
    } else {
        readInt("padTop", typography.padTop);
        readInt("padRight", typography.padRight);
        readInt("padBottom", typography.padBottom);
    }
}

void ReadUi(const Json& ui, UiOverrides& overrides, std::vector<std::string>& warnings)
{
    const auto assign = [&ui, &warnings](const char* key, std::optional<ThemeColor>& target) {
        if (const auto color = JsonColor(ui, key, warnings)) {
            target = *color;
        }
    };
    assign("windowBg", overrides.windowBg);
    assign("surface", overrides.surface);
    assign("navBg", overrides.navBg);
    assign("titleBarBg", overrides.titleBarBg);
    assign("titleBarInactive", overrides.titleBarInactive);
    assign("tabHover", overrides.tabHover);
    assign("buttonHover", overrides.buttonHover);
    assign("buttonPressed", overrides.buttonPressed);
    assign("divider", overrides.divider);
    assign("textPrimary", overrides.textPrimary);
    assign("textSecondary", overrides.textSecondary);
    assign("accent", overrides.accent);
    assign("accentSoft", overrides.accentSoft);
    assign("gutterBg", overrides.gutterBg);
    assign("currentLine", overrides.currentLine);
    assign("selection", overrides.selection);
    assign("danger", overrides.danger);
    assign("dirtyDot", overrides.dirtyDot);
}

} // namespace

std::string StripJsonComments(const std::string& text)
{
    std::string withoutComments;
    withoutComments.reserve(text.size());

    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < text.size();) {
        const char ch = text[i];
        if (inString) {
            withoutComments.push_back(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            ++i;
            continue;
        }
        if (ch == '"') {
            inString = true;
            withoutComments.push_back(ch);
            ++i;
            continue;
        }
        if (ch == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            i += 2;
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (ch == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
                ++i;
            }
            if (i + 1 < text.size()) {
                i += 2;
            }
            continue;
        }
        withoutComments.push_back(ch);
        ++i;
    }

    std::string result;
    result.reserve(withoutComments.size());
    inString = false;
    escaped = false;
    for (std::size_t i = 0; i < withoutComments.size(); ++i) {
        const char ch = withoutComments[i];
        if (inString) {
            result.push_back(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }
        if (ch == '"') {
            inString = true;
            result.push_back(ch);
            continue;
        }
        if (ch == ',') {
            std::size_t next = i + 1;
            while (next < withoutComments.size() &&
                   std::isspace(static_cast<unsigned char>(withoutComments[next])) != 0) {
                ++next;
            }
            if (next < withoutComments.size() &&
                (withoutComments[next] == '}' || withoutComments[next] == ']')) {
                continue;
            }
        }
        result.push_back(ch);
    }
    return result;
}

ThemeLoadResult LoadThemeSourceJson(const std::string& utf8, const std::string& sourceName)
{
    ThemeLoadResult result;
    constexpr std::size_t kMaxThemeJsonBytes = 2u * 1024u * 1024u;
    if (utf8.size() > kMaxThemeJsonBytes) {
        result.error = JoinContext(sourceName, "主题 JSON 超过 2 MB。");
        return result;
    }

    const std::string cleaned = StripJsonComments(utf8);
    const Json root = Json::parse(cleaned, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        result.error = JoinContext(sourceName, "主题文件不是有效的 JSON 对象。");
        return result;
    }

    ThemeSourceDraft draft;
    draft.schemaVersion = root.value("schemaVersion", 1);
    if (draft.schemaVersion > 1) {
        result.error = JoinContext(
            sourceName, "暂不支持 schemaVersion " + std::to_string(draft.schemaVersion) +
                            "（当前支持 1）。");
        return result;
    }
    if (draft.schemaVersion < 1) {
        result.warnings.push_back("schemaVersion 小于 1，按 1 处理。");
        draft.schemaVersion = 1;
    }

    draft.name = root.value("name", "");
    if (draft.name.empty()) {
        result.error = JoinContext(sourceName, "缺少 name 字段。");
        return result;
    }
    draft.id = root.value("id", "");
    draft.author = root.value("author", "");
    draft.license = root.value("license", "");
    draft.source = root.value("source", "");

    if (root.contains("dark")) {
        if (root["dark"].is_boolean()) {
            draft.dark = root["dark"].get<bool>();
        } else {
            result.warnings.push_back("dark 不是布尔值，已忽略。");
        }
    }
    if (root.contains("family") && root["family"].is_string()) {
        if (const auto family = FamilyFromString(root["family"].get<std::string>())) {
            draft.family = *family;
        } else {
            result.warnings.push_back("family 的值无法识别，已改为自动推断。");
        }
    }

    const Json* markdown = nullptr;
    if (root.contains("markdown") && root["markdown"].is_object()) {
        markdown = &root["markdown"];
    } else if (root.contains("colors") && root["colors"].is_object()) {
        markdown = &root["colors"];
    }
    if (markdown == nullptr) {
        result.error = JoinContext(sourceName, "缺少 markdown 颜色对象。");
        return result;
    }

    const auto read = [markdown, &result](const char* key) {
        return JsonColor(*markdown, key, result.warnings);
    };
    draft.background = read("background");
    draft.text = read("text");
    if (!draft.background || !draft.text) {
        result.error =
            JoinContext(sourceName, "markdown.background 和 markdown.text 为必填颜色。");
        return result;
    }
    draft.muted = read("muted");
    draft.accent = read("accent");
    draft.link = read("link");
    draft.border = read("border");
    draft.hr = read("hr");
    draft.strong = read("strong");
    draft.emphasis = read("emphasis");
    draft.listMarker = read("listMarker");
    draft.danger = read("danger");
    draft.selection = read("selection");
    draft.codeBackground = read("codeBackground");
    draft.codeText = read("codeText");
    draft.codeBorder = read("codeBorder");
    draft.tableBorder = read("tableBorder");
    draft.tableHeadBackground = read("tableHeadBackground");
    draft.findMatch = read("findMatch");
    draft.findCurrent = read("findCurrent");
    draft.findText = read("findText");
    draft.sidebarBackground = read("sidebarBackground");
    draft.sidebarHover = read("sidebarHover");

    if (markdown->contains("headings") && (*markdown)["headings"].is_array()) {
        const Json& headings = (*markdown)["headings"];
        for (std::size_t i = 0; i < 6 && i < headings.size(); ++i) {
            if (headings[i].is_string()) {
                if (const auto color =
                        ParseColor(headings[i].get<std::string>())) {
                    draft.headings[i] = *color;
                } else {
                    result.warnings.push_back("headings[" + std::to_string(i) +
                                              "] 的颜色值无效。");
                }
            }
        }
    }

    const auto nested = [&result](const Json& object, const char* key,
                                  std::optional<ThemeColor>& target) {
        if (const auto color = JsonColor(object, key, result.warnings)) {
            target = *color;
        }
    };

    if (markdown->contains("blockquote") && (*markdown)["blockquote"].is_object()) {
        nested((*markdown)["blockquote"], "text", draft.blockquoteText);
        nested((*markdown)["blockquote"], "border", draft.blockquoteBorder);
    }
    if (markdown->contains("code") && (*markdown)["code"].is_object()) {
        nested((*markdown)["code"], "background", draft.codeBackground);
        nested((*markdown)["code"], "text", draft.codeText);
        nested((*markdown)["code"], "border", draft.codeBorder);
    }
    if (markdown->contains("table") && (*markdown)["table"].is_object()) {
        nested((*markdown)["table"], "border", draft.tableBorder);
        nested((*markdown)["table"], "headBackground", draft.tableHeadBackground);
    }
    if (markdown->contains("find") && (*markdown)["find"].is_object()) {
        nested((*markdown)["find"], "match", draft.findMatch);
        nested((*markdown)["find"], "current", draft.findCurrent);
        nested((*markdown)["find"], "text", draft.findText);
    }
    if (markdown->contains("sidebar") && (*markdown)["sidebar"].is_object()) {
        nested((*markdown)["sidebar"], "background", draft.sidebarBackground);
        nested((*markdown)["sidebar"], "hover", draft.sidebarHover);
    }

    if (root.contains("tokens") && root["tokens"].is_object()) {
        const Json& tokensJson = root["tokens"];
        TokenColors tokens = {};
        bool present[10] = {};
        const auto readToken = [&tokensJson, &result, &present, &tokens](const char* key,
                                                                        int index,
                                                                        ThemeColor& target) {
            if (const auto color = JsonColor(tokensJson, key, result.warnings)) {
                target = *color;
                present[index] = true;
            }
        };
        readToken("comment", 0, tokens.comment);
        readToken("punctuation", 1, tokens.punctuation);
        readToken("tag", 2, tokens.tag);
        readToken("property", 3, tokens.property);
        readToken("booleanNumber", 4, tokens.booleanNumber);
        readToken("string", 5, tokens.string);
        readToken("operator", 6, tokens.oper);
        readToken("function", 7, tokens.function);
        readToken("keyword", 8, tokens.keyword);
        readToken("regex", 9, tokens.regex);

        const bool any = std::any_of(present, present + 10,
                                     [](bool value) { return value; });
        if (any) {
            ThemeSourceDraft baseDraft = draft;
            baseDraft.tokens.reset();
            const ThemeSource base = FinalizeSource(std::move(baseDraft));
            TokenColors merged = base.tokens;
            if (present[0]) {
                merged.comment = tokens.comment;
            }
            if (present[1]) {
                merged.punctuation = tokens.punctuation;
            }
            if (present[2]) {
                merged.tag = tokens.tag;
            }
            if (present[3]) {
                merged.property = tokens.property;
            }
            if (present[4]) {
                merged.booleanNumber = tokens.booleanNumber;
            }
            if (present[5]) {
                merged.string = tokens.string;
            }
            if (present[6]) {
                merged.oper = tokens.oper;
            }
            if (present[7]) {
                merged.function = tokens.function;
            }
            if (present[8]) {
                merged.keyword = tokens.keyword;
            }
            if (present[9]) {
                merged.regex = tokens.regex;
            }
            draft.tokens = merged;
        }
    }

    ReadTypography(root, draft.typography, result.warnings);

    if (root.contains("extraCss") && root["extraCss"].is_string()) {
        draft.extraCss = root["extraCss"].get<std::string>();
    } else if (root.contains("css") && root["css"].is_string()) {
        draft.extraCss = root["css"].get<std::string>();
    }
    std::vector<std::string> cssWarnings;
    draft.extraCss = SanitizeThemeCss(draft.extraCss, cssWarnings);
    result.warnings.insert(result.warnings.end(), cssWarnings.begin(), cssWarnings.end());

    if (root.contains("ui") && root["ui"].is_object()) {
        ReadUi(root["ui"], draft.ui, result.warnings);
    }

    if (root.contains("overrides") && root["overrides"].is_object()) {
        for (auto it = root["overrides"].begin(); it != root["overrides"].end(); ++it) {
            if (!it.value().is_string()) {
                result.warnings.push_back("overrides." + it.key() + " 不是字符串。");
                continue;
            }
            const std::optional<ThemeColor> color =
                ParseColor(it.value().get<std::string>());
            if (!color) {
                result.warnings.push_back("overrides." + it.key() + " 的颜色值无效。");
                continue;
            }
            const std::string key = it.key();
            if (key.rfind("chrome.", 0) == 0 || key.rfind("preview.", 0) == 0 ||
                key.rfind("editor.", 0) == 0) {
                draft.overrides[key] = *color;
            } else {
                result.warnings.push_back("忽略未知 overrides 键：" + key);
            }
        }
    }

    result.source = FinalizeSource(std::move(draft));
    return result;
}

ThemeLoadResult LoadThemeSourceFile(const std::filesystem::path& path)
{
    ThemeLoadResult result;
    std::error_code ec;
    const std::optional<std::string> text = readFile(path.wstring(), ec);
    if (!text) {
        result.error = "无法读取主题文件：" + path.string() + " (" + ec.message() + ")";
        return result;
    }
    return LoadThemeSourceJson(*text, path.filename().string());
}

std::string ThemeSourceToJson(const ThemeSource& source)
{
    Json root;
    root["schemaVersion"] = source.schemaVersion;
    if (!source.id.empty()) {
        root["id"] = source.id;
    }
    root["name"] = source.name;
    if (!source.author.empty()) {
        root["author"] = source.author;
    }
    if (!source.license.empty()) {
        root["license"] = source.license;
    }
    if (!source.source.empty()) {
        root["source"] = source.source;
    }
    root["dark"] = source.dark;
    root["family"] = FamilyToString(source.family);

    Json markdown;
    markdown["background"] = ColorToCss(source.background);
    markdown["text"] = ColorToCss(source.text);
    markdown["muted"] = ColorToCss(source.muted);
    markdown["accent"] = ColorToCss(source.accent);
    markdown["link"] = ColorToCss(source.link);
    markdown["border"] = ColorToCss(source.border);
    markdown["hr"] = ColorToCss(source.hr);
    markdown["strong"] = ColorToCss(source.strong);
    markdown["emphasis"] = ColorToCss(source.emphasis);
    markdown["listMarker"] = ColorToCss(source.listMarker);
    Json headings = Json::array();
    for (const ThemeColor& color : source.headings) {
        headings.push_back(ColorToCss(color));
    }
    markdown["headings"] = std::move(headings);
    markdown["blockquote"] = {
        {"text", ColorToCss(source.blockquoteText)},
        {"border", ColorToCss(source.blockquoteBorder)},
    };
    markdown["code"] = {
        {"background", ColorToCss(source.codeBackground)},
        {"text", ColorToCss(source.codeText)},
        {"border", ColorToCss(source.codeBorder)},
    };
    markdown["table"] = {
        {"border", ColorToCss(source.tableBorder)},
        {"headBackground", ColorToCss(source.tableHeadBackground)},
    };
    markdown["selection"] = ColorToCss(source.selection);
    markdown["find"] = {
        {"match", ColorToCss(source.findMatch)},
        {"current", ColorToCss(source.findCurrent)},
        {"text", ColorToCss(source.findText)},
    };
    markdown["sidebar"] = {
        {"background", ColorToCss(source.sidebarBackground)},
        {"hover", ColorToCss(source.sidebarHover)},
    };
    markdown["danger"] = ColorToCss(source.danger);
    root["markdown"] = std::move(markdown);

    root["tokens"] = {
        {"comment", ColorToCss(source.tokens.comment)},
        {"punctuation", ColorToCss(source.tokens.punctuation)},
        {"tag", ColorToCss(source.tokens.tag)},
        {"property", ColorToCss(source.tokens.property)},
        {"booleanNumber", ColorToCss(source.tokens.booleanNumber)},
        {"string", ColorToCss(source.tokens.string)},
        {"operator", ColorToCss(source.tokens.oper)},
        {"function", ColorToCss(source.tokens.function)},
        {"keyword", ColorToCss(source.tokens.keyword)},
        {"regex", ColorToCss(source.tokens.regex)},
    };

    root["typography"] = {
        {"bodyFont", source.typography.bodyFont},
        {"codeFont", source.typography.codeFont},
        {"bodySize", source.typography.bodySize},
        {"lineHeight", source.typography.lineHeight},
        {"maxWidth", source.typography.maxWidth},
        {"padding", {source.typography.padTop, source.typography.padRight,
                     source.typography.padBottom}},
        {"codeSize", source.typography.codeSize},
    };

    if (!source.extraCss.empty()) {
        root["extraCss"] = source.extraCss;
    }

    if (source.ui.any()) {
        Json ui = Json::object();
        const auto put = [&ui](const char* key, const std::optional<ThemeColor>& color) {
            if (color) {
                ui[key] = ColorToCss(*color);
            }
        };
        put("windowBg", source.ui.windowBg);
        put("surface", source.ui.surface);
        put("navBg", source.ui.navBg);
        put("titleBarBg", source.ui.titleBarBg);
        put("titleBarInactive", source.ui.titleBarInactive);
        put("tabHover", source.ui.tabHover);
        put("buttonHover", source.ui.buttonHover);
        put("buttonPressed", source.ui.buttonPressed);
        put("divider", source.ui.divider);
        put("textPrimary", source.ui.textPrimary);
        put("textSecondary", source.ui.textSecondary);
        put("accent", source.ui.accent);
        put("accentSoft", source.ui.accentSoft);
        put("gutterBg", source.ui.gutterBg);
        put("currentLine", source.ui.currentLine);
        put("selection", source.ui.selection);
        put("danger", source.ui.danger);
        put("dirtyDot", source.ui.dirtyDot);
        root["ui"] = std::move(ui);
    }

    if (!source.overrides.empty()) {
        Json overrides = Json::object();
        for (const auto& [key, color] : source.overrides) {
            overrides[key] = ColorToCss(color);
        }
        root["overrides"] = std::move(overrides);
    }

    return root.dump(2);
}

bool SaveThemeSourceFile(const std::filesystem::path& path, const ThemeSource& source,
                         std::string& error)
{
    std::error_code ec;
    if (!writeFile(path.wstring(), ThemeSourceToJson(source), ec)) {
        error = ec.message();
        return false;
    }
    return true;
}

} // namespace awm::theme
