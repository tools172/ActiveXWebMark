#include "markdown/previewhtml.h"

#include "markdown/previewcss.h"

namespace awm {

namespace {

// IE11 fails with "regex syntax error" when a RegExp object is passed to the
// RegExp constructor together with flags. Prism 1.29 calls that form from
// setLanguage, so replace the implementation with a string-source version.
constexpr const char* kPrismPatchScript = R"JS(
(function(){
if(!window.Prism||!Prism.util||!Prism.util.setLanguage){return;}
try{
var source=/(?:^|\s)lang(?:uage)?-([\w-]+)(?=\s|$)/i.source;
Prism.util.setLanguage=function(el,lang){
el.className=el.className.replace(new RegExp(source,'gi'),'');
if(el.classList&&el.classList.add){el.classList.add('language-'+lang);}
else{el.className+=' language-'+lang;}
};
}catch(e){}
})();
)JS";

// Runs after the Prism bundle and patch above. Only code elements with a known
// language class are touched; unknown languages and plain code stay untouched.
// The character caps keep IE11 responsive on pathologically large code blocks.
constexpr const char* kPrismInitScript = R"JS(
(function(){
if(!window.Prism||!Prism.languages){return;}
var alias={'c++':'cpp','cc':'cpp','cxx':'cpp','hpp':'cpp','h':'c',
'js':'javascript','ts':'typescript','jsx':'jsx','tsx':'tsx',
'html':'markup','xml':'markup','svg':'markup',
'py':'python','sh':'bash','shell':'bash','zsh':'bash',
'yml':'yaml','cs':'csharp','c#':'csharp','dotnet':'csharp',
'md':'markdown','ps1':'powershell','pwsh':'powershell',
'dockerfile':'docker'};
var re=/(?:^|\s)language-([^\s]+)/i;
var codes=document.getElementsByTagName('code');
var budget=256*1024;
for(var i=0;i<codes.length;i++){
var el=codes[i];
var m=re.exec(el.className||'');
if(!m){continue;}
var raw=m[1].toLowerCase();
var lang=alias[raw]||raw;
var grammar=Prism.languages[lang];
if(!grammar||typeof grammar!=='object'){continue;}
var text=el.textContent||el.innerText||'';
if(text.length>64*1024||budget<=0){continue;}
budget-=text.length;
el.className=el.className.replace(re,' language-'+lang);
try{Prism.highlightElement(el);}catch(e){}
}
})();
)JS";

bool startsWithNoCase(std::string_view text, std::size_t offset, std::string_view prefix)
{
    if (offset + prefix.size() > text.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const char a = text[offset + i];
        const char b = prefix[i];
        const char lowerA = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
        const char lowerB = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
        if (lowerA != lowerB) {
            return false;
        }
    }
    return true;
}

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

std::string EscapeInlineScript(std::string_view script)
{
    static constexpr std::string_view kEndTag = "</script";
    std::string escaped;
    escaped.reserve(script.size() + 16);

    std::size_t pos = 0;
    while (pos < script.size()) {
        if (startsWithNoCase(script, pos, kEndTag)) {
            escaped += "<\\/script";
            pos += kEndTag.size();
        } else {
            escaped.push_back(script[pos]);
            ++pos;
        }
    }
    return escaped;
}

std::string BuildPreviewHtml(const std::string& bodyHtml, const PreviewHtmlOptions& options)
{
    const theme::ThemeDefinition& theme =
        options.theme != nullptr ? *options.theme : theme::DefaultTheme();
    const std::string css = BuildPreviewCss(theme);

    const bool hasCode = bodyHtml.find("language-") != std::string::npos;
    const bool injectScript = hasCode && !options.inlineScript.empty();

    std::string html;
    html.reserve(bodyHtml.size() + css.size() + options.extraCss.size() +
                 (injectScript ? options.inlineScript.size() : 0) + 4096);

    html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    html += "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";

    if (!options.baseFileUrl.empty()) {
        html += "<base href=\"";
        html += EscapeHtmlAttribute(options.baseFileUrl);
        html += "\">";
    }

    html += "<style>";
    html += css;
    if (!options.extraCss.empty()) {
        html += options.extraCss;
    }
    html += "</style></head><body><div class=\"markdown-body\">";
    html += bodyHtml;
    html += "</div>";

    if (injectScript) {
        html += "<script>window.Prism={manual:true};</script>";
        html += "<script>";
        html += EscapeInlineScript(options.inlineScript);
        html += "</script>";
        html += "<script>";
        html += EscapeInlineScript(kPrismPatchScript);
        html += "</script>";
        html += "<script>";
        html += EscapeInlineScript(kPrismInitScript);
        html += "</script>";
    }

    html += "</body></html>";
    return html;
}

} // namespace awm
