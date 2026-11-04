#include "markdown/previewcss.h"

namespace awm {

namespace {

using theme::ColorToCss;
using theme::ThemeColor;

void appendRule(std::string& css, const char* selector, const ThemeColor& color)
{
    css += selector;
    css += "{color:";
    css += ColorToCss(color);
    css += ";}";
}

} // namespace

std::string BuildPreviewCss(const theme::ThemeDefinition& theme)
{
    const theme::PreviewColors& preview = theme.preview;
    const theme::TokenColors& tokens = theme.tokens;

    std::string css;
    css.reserve(4096);

    css += "html,body{margin:0;padding:0;}";
    css += "body{background:";
    css += ColorToCss(preview.pageBg);
    css += ";color:";
    css += ColorToCss(preview.pageText);
    css += ";font-family:'Segoe UI','Microsoft YaHei UI','Microsoft YaHei',sans-serif;"
           "font-size:15px;line-height:1.75;-webkit-text-size-adjust:100%;}";
    css += ".markdown-body{max-width:980px;margin:0 auto;padding:32px 40px 64px;"
           "overflow-wrap:break-word;word-wrap:break-word;}";
    css += "h1,h2,h3,h4,h5,h6{font-weight:600;line-height:1.3;margin:28px 0 14px;}";

    const auto appendHeading = [&](const char* selector, const ThemeColor& color,
                                   const char* size, bool border) {
        css += selector;
        css += "{color:";
        css += ColorToCss(color);
        css += ";font-size:";
        css += size;
        css += ";";
        if (border) {
            css += "border-bottom:1px solid ";
            css += ColorToCss(preview.border);
            css += ";padding-bottom:";
            css += selector[1] == '1' ? ".3em" : ".25em";
            css += ";";
        }
        css += "}";
    };
    appendHeading("h1", preview.headings[0], "2em", true);
    appendHeading("h2", preview.headings[1], "1.55em", true);
    appendHeading("h3", preview.headings[2], "1.3em", false);
    appendHeading("h4", preview.headings[3], "1.12em", false);
    appendHeading("h5", preview.headings[4], "1em", false);
    appendHeading("h6", preview.headings[5], ".92em", false);

    css += "p,ul,ol,blockquote,pre,table{margin:0 0 16px;}";
    css += "ul,ol{padding-left:2em;}li+li{margin-top:.25em;}";
    css += "a{color:";
    css += ColorToCss(preview.link);
    css += ";text-decoration:none;}a:hover{text-decoration:underline;}";
    css += "strong{font-weight:600;color:";
    css += ColorToCss(preview.strong);
    css += ";}em{color:";
    css += ColorToCss(preview.em);
    css += ";}";
    css += "code{font-family:Consolas,'Cascadia Mono','Courier New',monospace;"
           "font-size:90%;background:";
    css += ColorToCss(preview.inlineCodeBg);
    css += ";color:";
    css += ColorToCss(preview.inlineCodeText);
    css += ";padding:2px 5px;border-radius:4px;}";
    css += "pre{background:";
    css += ColorToCss(preview.codeBlockBg);
    css += ";border:1px solid ";
    css += ColorToCss(preview.codeBlockBorder);
    css += ";border-radius:6px;padding:14px 16px;overflow:auto;line-height:1.5;}";
    css += "pre code{background:none;padding:0;font-size:13px;color:";
    css += ColorToCss(preview.codeBlockText);
    css += ";}";
    css += "blockquote{border-left:4px solid ";
    css += ColorToCss(preview.blockquoteBorder);
    css += ";color:";
    css += ColorToCss(preview.blockquoteText);
    css += ";margin-left:0;padding:0 16px;}";
    css += "table{border-collapse:collapse;max-width:100%;}";
    css += "th,td{border:1px solid ";
    css += ColorToCss(preview.tableBorder);
    css += ";padding:6px 13px;}";
    css += "th{background:";
    css += ColorToCss(preview.tableHeadBg);
    css += ";font-weight:600;}";
    css += "img{max-width:100%;height:auto;box-sizing:border-box;}";
    css += "hr{height:1px;border:0;background:";
    css += ColorToCss(preview.hr);
    css += ";margin:24px 0;}";
    css += "kbd{font-family:Consolas,monospace;font-size:85%;border:1px solid ";
    css += ColorToCss(preview.tableBorder);
    css += ";border-bottom-width:2px;border-radius:4px;padding:1px 5px;background:";
    css += ColorToCss(preview.inlineCodeBg);
    css += ";}";
    css += "::selection{background:";
    css += ColorToCss(preview.selectionBg);
    css += ";}";

    // Preview find highlights. The find scripts only toggle these classes; no
    // inline colors are written, so dark themes stay readable.
    css += ".awm-find{background:";
    css += ColorToCss(preview.findMatchBg);
    css += ";color:";
    css += ColorToCss(preview.findText);
    css += ";border-radius:2px;}";
    css += ".awm-find-current{background:";
    css += ColorToCss(preview.findCurrentBg);
    css += ";}";

    // Prism token colors. Grouped exactly like MarkText's Prism theme files so
    // the borrowed palettes keep their intended balance.
    css += "code[class*=\"language-\"],pre code{color:";
    css += ColorToCss(preview.codeBlockText);
    css += ";}";
    appendRule(css, ".token.comment,.token.prolog,.token.doctype,.token.cdata",
               tokens.comment);
    appendRule(css, ".token.punctuation", tokens.punctuation);
    appendRule(css, ".token.selector,.token.tag", tokens.tag);
    appendRule(css, ".token.property,.token.constant,.token.symbol,.token.deleted",
               tokens.property);
    appendRule(css, ".token.boolean,.token.number", tokens.booleanNumber);
    appendRule(css,
               ".token.string,.token.char,.token.attr-name,.token.attr-value,"
               ".token.builtin,.token.inserted",
               tokens.string);
    appendRule(css,
               ".token.operator,.token.entity,.token.url,.language-css .token.string,"
               ".style .token.string",
               tokens.oper);
    appendRule(css, ".token.atrule,.token.function,.token.class-name", tokens.function);
    appendRule(css, ".token.keyword", tokens.keyword);
    appendRule(css, ".token.regex,.token.important,.token.variable", tokens.regex);
    css += ".token.important,.token.bold{font-weight:bold;}";
    css += ".token.italic{font-style:italic;}";
    css += ".token.entity{cursor:help;}";
    css += ".token.namespace{opacity:.7;}";

    return css;
}

} // namespace awm
