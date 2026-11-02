#include "markdown/previewhtml.h"

namespace awm {

namespace {

constexpr const char* kStylesheet =
    "html,body{margin:0;padding:0;}"
    "body{background:#ffffff;color:#1f2328;"
    "font-family:'Segoe UI','Microsoft YaHei UI','Microsoft YaHei',sans-serif;"
    "font-size:15px;line-height:1.75;-webkit-text-size-adjust:100%;}"
    ".markdown-body{max-width:980px;margin:0 auto;padding:32px 40px 64px;"
    "overflow-wrap:break-word;word-wrap:break-word;}"
    "h1,h2,h3,h4,h5,h6{font-weight:600;line-height:1.3;margin:28px 0 14px;}"
    "h1{font-size:2em;border-bottom:1px solid #eaecef;padding-bottom:.3em;}"
    "h2{font-size:1.55em;border-bottom:1px solid #eaecef;padding-bottom:.25em;}"
    "h3{font-size:1.3em;}h4{font-size:1.12em;}h5{font-size:1em;}h6{font-size:.92em;color:#57606a;}"
    "p,ul,ol,blockquote,pre,table{margin:0 0 16px;}"
    "ul,ol{padding-left:2em;}"
    "li+li{margin-top:.25em;}"
    "a{color:#0969da;text-decoration:none;}a:hover{text-decoration:underline;}"
    "strong{font-weight:600;}"
    "code{font-family:Consolas,'Cascadia Mono','Courier New',monospace;font-size:90%;"
    "background:#f6f8fa;padding:2px 5px;border-radius:4px;}"
    "pre{background:#f6f8fa;border:1px solid #eef0f3;border-radius:6px;padding:14px 16px;"
    "overflow:auto;line-height:1.5;}"
    "pre code{background:none;padding:0;font-size:13px;}"
    "blockquote{border-left:4px solid #d0d7de;color:#57606a;margin-left:0;"
    "padding:0 16px;}"
    "table{border-collapse:collapse;max-width:100%;}"
    "th,td{border:1px solid #d0d7de;padding:6px 13px;}"
    "th{background:#f6f8fa;font-weight:600;}"
    "img{max-width:100%;height:auto;box-sizing:border-box;}"
    "hr{height:1px;border:0;background:#e5e7eb;margin:24px 0;}"
    "kbd{font-family:Consolas,monospace;font-size:85%;border:1px solid #d0d7de;"
    "border-bottom-width:2px;border-radius:4px;padding:1px 5px;background:#f6f8fa;}";

} // namespace

std::string EscapeHtmlAttribute(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':  escaped += "&amp;"; break;
        case '<':  escaped += "&lt;"; break;
        case '>':  escaped += "&gt;"; break;
        case '"':  escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default:   escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string BuildPreviewHtml(const std::string& bodyHtml, const PreviewHtmlOptions& options)
{
    std::string html;
    html.reserve(bodyHtml.size() + 4096);

    html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    html += "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";

    if (!options.baseFileUrl.empty()) {
        html += "<base href=\"";
        html += EscapeHtmlAttribute(options.baseFileUrl);
        html += "\">";
    }

    html += "<style>";
    html += kStylesheet;
    if (!options.extraCss.empty()) {
        html += options.extraCss;
    }
    html += "</style></head><body><div class=\"markdown-body\">";
    html += bodyHtml;
    html += "</div></body></html>";
    return html;
}

} // namespace awm
