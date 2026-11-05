#include "theme/theme_import_internal.h"

#include <cctype>
#include <map>
#include <optional>
#include <system_error>

#include "util/fileutil.h"

namespace awm::theme::detail {

namespace {

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

std::string LowerAscii(std::string text)
{
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::string Unquote(std::string value)
{
    value = std::string(Trim(value));
    if (value.empty()) {
        return {};
    }
    const char quote = value.front();
    if (quote == '"' || quote == '\'') {
        const std::size_t close = value.find(quote, 1);
        if (close != std::string::npos) {
            return value.substr(1, close - 1);
        }
    }
    // Trailing YAML comment (" #..." outside quotes).
    const std::size_t comment = value.find(" #");
    if (comment != std::string::npos) {
        value = value.substr(0, comment);
    }
    return std::string(Trim(value));
}

std::optional<ThemeColor> ColorAt(const std::map<std::string, std::string>& values,
                                  const char* key)
{
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        return std::nullopt;
    }
    std::string text = it->second;
    if (text.front() != '#') {
        text.insert(text.begin(), '#');
    }
    return ParseColor(text);
}

ThemeColor WithAlpha(const ThemeColor& color, int alpha)
{
    return Rgba(color.r, color.g, color.b, alpha);
}

} // namespace

ImportResult ImportBase16File(const std::filesystem::path& path)
{
    ImportResult result;
    std::error_code ec;
    const std::optional<std::string> text = readFile(path.wstring(), ec);
    if (!text) {
        result.error = "无法读取 Base16 文件：" + path.string() + " (" + ec.message() + ")";
        return result;
    }
    constexpr std::size_t kMaxBase16Bytes = 1024u * 1024u;
    if (text->size() > kMaxBase16Bytes) {
        result.error = "Base16 文件超过 1 MB。";
        return result;
    }

    std::map<std::string, std::string> values;
    std::size_t start = 0;
    while (start <= text->size()) {
        const std::size_t end = text->find('\n', start);
        const std::string line =
            text->substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::string_view view = Trim(line);
        if (!view.empty() && view.front() != '#') {
            const std::size_t colon = view.find(':');
            if (colon != std::string_view::npos) {
                const std::string key = LowerAscii(std::string(Trim(view.substr(0, colon))));
                const std::string value = Unquote(std::string(Trim(view.substr(colon + 1))));
                if (!key.empty() && !value.empty()) {
                    values[key] = value;
                }
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    const auto base00 = ColorAt(values, "base00");
    const auto base05 = ColorAt(values, "base05");
    if (!base00 || !base05) {
        result.error = "Base16 配色缺少 base00/base05。";
        return result;
    }

    ThemeSourceDraft draft;
    draft.name = values.count("name") != 0 ? values["name"] : path.stem().string();
    draft.author = values.count("author") != 0 ? values["author"] : "";
    draft.source = "Base16/Base24: " + path.filename().string();
    if (values.count("variant") != 0) {
        const std::string variant = LowerAscii(values["variant"]);
        if (variant == "dark" || variant == "light") {
            draft.dark = variant == "dark";
        }
    }

    const auto base = [&values](const char* key) {
        return ColorAt(values, key);
    };
    const auto base01 = base("base01");
    const auto base02 = base("base02");
    const auto base03 = base("base03");
    const auto base04 = base("base04");
    const auto base08 = base("base08");
    const auto base09 = base("base09");
    const auto base0A = base("base0a");
    const auto base0B = base("base0b");
    const auto base0C = base("base0c");
    const auto base0D = base("base0d");
    const auto base0E = base("base0e");
    const auto base0F = base("base0f");

    draft.background = *base00;
    draft.text = *base05;
    draft.muted = base03;
    draft.accent = base0D;
    draft.link = base0D;
    draft.border = base01;
    draft.hr = base01;
    draft.strong = base("base06").value_or(*base05);
    draft.emphasis = base0A;
    draft.listMarker = base04;
    draft.codeBackground = base01;
    draft.codeText = base05;
    draft.codeBorder = base01;
    draft.tableBorder = base01;
    draft.tableHeadBackground = base01;
    draft.selection = base02;
    if (base0A) {
        draft.findMatch = WithAlpha(*base0A, 102);
        draft.findCurrent = WithAlpha(*base0A, 153);
    }
    draft.findText = base00;
    draft.sidebarBackground = base01;
    draft.sidebarHover = base02;
    draft.danger = base08;
    for (auto& heading : draft.headings) {
        heading = base05;
    }
    if (base0D) {
        draft.headings[0] = base0D;
    }

    TokenColors tokens{};
    tokens.comment = base03.value_or(*base05);
    tokens.punctuation = *base05;
    tokens.tag = base08.value_or(*base05);
    tokens.property = base0D.value_or(*base05);
    tokens.booleanNumber = base09.value_or(*base05);
    tokens.string = base0B.value_or(*base05);
    tokens.oper = base0C.value_or(*base05);
    tokens.function = base0D.value_or(*base05);
    tokens.keyword = base0E.value_or(*base05);
    tokens.regex = base0F.value_or(base09.value_or(*base05));
    draft.tokens = tokens;

    ImportedTheme imported;
    imported.source = FinalizeSource(std::move(draft));
    result.themes.push_back(std::move(imported));
    return result;
}

} // namespace awm::theme::detail
