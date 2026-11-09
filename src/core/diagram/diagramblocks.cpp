#include "diagram/diagramblocks.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <utility>

#include "markdown/previewhtml.h"
#include "util/pathutil.h"

namespace awm {

namespace {

constexpr std::size_t kNpos = std::string::npos;

std::string ToLower(std::string value)
{
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

// True when a tag name starts at `pos`; the next byte must end the name.
bool IsTagAt(const std::string& html, std::size_t pos, const char* name)
{
    const std::size_t length = std::strlen(name);
    if (pos + length > html.size()) {
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        if (std::tolower(static_cast<unsigned char>(html[pos + i])) != name[i]) {
            return false;
        }
    }
    if (pos + length == html.size()) {
        return true;
    }
    const unsigned char next = static_cast<unsigned char>(html[pos + length]);
    return next == '>' || std::isspace(next) != 0;
}

std::string ExtractAttribute(const std::string& tag, const char* name)
{
    const std::string lower = ToLower(tag);
    std::string needle = ToLower(std::string(name));
    needle += "=\"";
    const std::size_t pos = lower.find(needle);
    if (pos == kNpos) {
        return {};
    }
    const std::size_t start = pos + needle.size();
    const std::size_t end = tag.find('"', start);
    if (end == kNpos) {
        return {};
    }
    return tag.substr(start, end - start);
}

// cmark escapes code-block text; decode the entities it emits.
std::string HtmlUnescape(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size();) {
        if (value[i] != '&') {
            out.push_back(value[i++]);
            continue;
        }
        const std::size_t semicolon = value.find(';', i + 1);
        if (semicolon == kNpos || semicolon - i > 10) {
            out.push_back(value[i++]);
            continue;
        }

        const std::string entity = value.substr(i + 1, semicolon - i - 1);
        if (entity == "amp") {
            out.push_back('&');
        } else if (entity == "lt") {
            out.push_back('<');
        } else if (entity == "gt") {
            out.push_back('>');
        } else if (entity == "quot") {
            out.push_back('"');
        } else if (entity == "apos" || entity == "#39" || entity == "#x27" ||
                   entity == "#X27") {
            out.push_back('\'');
        } else {
            out.append(value, i, semicolon - i + 1);
        }
        i = semicolon + 1;
    }
    return out;
}

bool IsCached(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return false;
    }
    const auto size = std::filesystem::file_size(path, ec);
    return !ec && size > 0;
}

std::string SourcePosAttribute(const std::string& preTag)
{
    const std::string value = ExtractAttribute(preTag, "data-sourcepos");
    if (value.empty()) {
        return {};
    }
    return " data-sourcepos=\"" + EscapeHtmlAttribute(value) + "\"";
}

struct Match {
    bool matched = false;
    DiagramKind kind = DiagramKind::Mermaid;
    std::string codeClass;
    std::string variant;
    std::string version;
};

Match MatchDiagram(const std::string& lowerCodeTag,
                   const DiagramTransformOptions& options)
{
    if (options.mermaidEnabled &&
        lowerCodeTag.find("language-mermaid") != kNpos) {
        return {true, DiagramKind::Mermaid, "language-mermaid",
                options.mermaidTheme, options.mermaidVersion};
    }
    if (options.plantumlEnabled &&
        (lowerCodeTag.find("language-plantuml") != kNpos ||
         lowerCodeTag.find("language-puml") != kNpos)) {
        const bool native = options.plantumlMode == "native";
        const std::string appearance = options.plantumlDark ? ":dark" : ":light";
        const std::string variant =
            native ? "native:" + options.plantumlExe + appearance
                   : "remote:" + options.plantumlServer + appearance;
        return {true, DiagramKind::PlantUml, "language-plantuml", variant,
                options.plantumlVersion};
    }
    return {};
}

} // namespace

const char* DiagramKindName(DiagramKind kind)
{
    return kind == DiagramKind::PlantUml ? "plantuml" : "mermaid";
}

std::string DiagramCacheKey(DiagramKind kind, const std::string& source,
                            const std::string& variant, const std::string& version)
{
    // FNV-1a 64: stable across builds and good enough for a local cache key.
    std::uint64_t hash = 14695981039346656037ull;
    const auto feed = [&hash](const std::string& text) {
        for (const unsigned char byte : text) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
    };
    feed("awm-diagram-cache-v1\nkind=");
    feed(DiagramKindName(kind));
    feed("\nvariant=");
    feed(variant);
    feed("\nversion=");
    feed(version);
    feed("\nsource=");
    feed(source);

    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%016llx",
                  static_cast<unsigned long long>(hash));
    return buffer;
}

DiagramTransformResult TransformDiagramBlocks(
    const std::string& bodyHtml, const DiagramTransformOptions& options)
{
    DiagramTransformResult result;
    if ((!options.mermaidEnabled && !options.plantumlEnabled) ||
        options.cacheDirectory.empty()) {
        result.html = bodyHtml;
        return result;
    }

    result.html.reserve(bodyHtml.size() + 1024);

    std::size_t pos = 0;
    std::size_t search = 0;
    std::size_t diagramIndex = 0;
    const std::string& html = bodyHtml;

    while (search < html.size()) {
        const std::size_t pre = html.find("<pre", search);
        if (pre == kNpos) {
            break;
        }
        if (!IsTagAt(html, pre + 1, "pre")) {
            search = pre + 4;
            continue;
        }

        const std::size_t preTagEnd = html.find('>', pre + 4);
        if (preTagEnd == kNpos) {
            break;
        }
        const std::size_t preClose = html.find("</pre>", preTagEnd + 1);
        if (preClose == kNpos) {
            break;
        }

        const std::size_t code = html.find("<code", preTagEnd + 1);
        if (code == kNpos || code >= preClose) {
            search = preClose + 6;
            continue;
        }
        const std::size_t codeTagEnd = html.find('>', code + 5);
        if (codeTagEnd == kNpos || codeTagEnd >= preClose) {
            search = preClose + 6;
            continue;
        }

        const std::string codeTag = html.substr(code, codeTagEnd - code + 1);
        const Match match = MatchDiagram(ToLower(codeTag), options);
        if (!match.matched) {
            search = preClose + 6;
            continue;
        }

        const std::size_t codeClose = html.find("</code>", codeTagEnd + 1);
        if (codeClose == kNpos || codeClose >= preClose || codeClose + 7 > preClose) {
            search = preClose + 6;
            continue;
        }
        // cmark emits </code></pre> directly; tolerate only whitespace between
        // them so unexpected markup is left untouched.
        const std::string trailing =
            html.substr(codeClose + 7, preClose - (codeClose + 7));
        if (trailing.find_first_not_of(" \t\r\n") != kNpos) {
            search = preClose + 6;
            continue;
        }

        const std::string escapedSource =
            html.substr(codeTagEnd + 1, codeClose - codeTagEnd - 1);
        if (escapedSource.size() > options.maxSourceBytes) {
            search = preClose + 6;
            continue;
        }
        const std::string source = HtmlUnescape(escapedSource);
        if (source.size() > options.maxSourceBytes) {
            search = preClose + 6;
            continue;
        }

        const std::string preTag = html.substr(pre, preTagEnd - pre + 1);
        const std::string sourcePos = SourcePosAttribute(preTag);
        const std::string cacheKey =
            DiagramCacheKey(match.kind, source, match.variant, match.version);
        const std::filesystem::path cachePath =
            options.cacheDirectory / (cacheKey + ".svg");
        const std::string url = fileUrlFromPath(cachePath.wstring());
        const std::string kindName = DiagramKindName(match.kind);

        result.html.append(html, pos, pre - pos);
        if (IsCached(cachePath)) {
            result.html += "<div class=\"awm-diagram\" data-diagram-kind=\"";
            result.html += kindName;
            result.html += "\"";
            result.html += sourcePos;
            result.html += "><img src=\"";
            result.html += EscapeHtmlAttribute(url);
            result.html += "\" alt=\"";
            result.html += match.kind == DiagramKind::Mermaid ? "Mermaid diagram"
                                                               : "PlantUML diagram";
            result.html += "\"></div>";
        } else {
            const std::string elementId = "awm-diagram-" + kindName + "-" +
                                          cacheKey + "-" +
                                          std::to_string(diagramIndex);
            result.html += "<div class=\"awm-diagram\" id=\"";
            result.html += elementId;
            result.html += "\" data-diagram-kind=\"";
            result.html += kindName;
            result.html += "\"";
            result.html += sourcePos;
            result.html += "><pre><code class=\"";
            result.html += match.codeClass;
            result.html += "\">";
            result.html += escapedSource;
            result.html += "</code></pre><div class=\"awm-diagram-status\">"
                           "正在渲染图表…</div></div>";

            DiagramPendingBlock pending;
            pending.kind = match.kind;
            pending.elementId = elementId;
            pending.source = source;
            if (match.kind == DiagramKind::Mermaid) {
                pending.theme = options.mermaidTheme;
            } else {
                const bool native = options.plantumlMode == "native";
                pending.mode = native ? "native" : "remote";
                pending.server = native ? std::string() : options.plantumlServer;
                pending.exe = native ? options.plantumlExe : std::string();
                pending.dark = options.plantumlDark;
            }
            pending.cachePath = cachePath;
            pending.url = url;
            result.pending.push_back(std::move(pending));
        }

        ++diagramIndex;
        pos = preClose + 6;
        search = pos;
    }

    result.html.append(html, pos, kNpos);
    return result;
}

} // namespace awm
