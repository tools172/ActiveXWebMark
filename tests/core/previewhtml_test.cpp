#include <gtest/gtest.h>

#include <string>

#include "markdown/previewhtml.h"
#include "theme/theme_catalog.h"

using namespace awm;

TEST(PreviewHtml, ProducesCompleteDocument)
{
    const std::string html = BuildPreviewHtml("<h1 id=\"t\">Title</h1>", {});
    EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(html.find("<meta charset=\"utf-8\">"), std::string::npos);
    EXPECT_NE(html.find("IE=edge"), std::string::npos);
    EXPECT_NE(html.find("<h1 id=\"t\">Title</h1>"), std::string::npos);
    EXPECT_NE(html.find("<div class=\"markdown-body\">"), std::string::npos);
}

TEST(PreviewHtml, EmitsEscapedBaseHref)
{
    PreviewHtmlOptions options;
    options.baseFileUrl = "file:///C:/docs/?a=1&b=2";
    const std::string html = BuildPreviewHtml("body", options);

    EXPECT_NE(html.find("<base href=\"file:///C:/docs/?a=1&amp;b=2\">"), std::string::npos);
}

TEST(PreviewHtml, OmitsBaseWhenEmpty)
{
    const std::string html = BuildPreviewHtml("body", {});
    EXPECT_EQ(html.find("<base "), std::string::npos);
}

TEST(PreviewHtml, AppendsExtraCss)
{
    PreviewHtmlOptions options;
    options.extraCss = "body{background:#fafafa;}";
    const std::string html = BuildPreviewHtml("body", options);
    EXPECT_NE(html.find("body{background:#fafafa;}"), std::string::npos);
}

TEST(PreviewHtml, EscapesAttributeCharacters)
{
    EXPECT_EQ(EscapeHtmlAttribute("a&b<c>\"d\"'e'"),
              "a&amp;b&lt;c&gt;&quot;d&quot;&#39;e&#39;");
}

TEST(PreviewHtml, UsesSelectedThemeColors)
{
    PreviewHtmlOptions options;
    options.theme = theme::FindTheme("dracula");
    ASSERT_NE(options.theme, nullptr);
    const std::string html = BuildPreviewHtml("body", options);

    EXPECT_NE(html.find(theme::ColorToCss(options.theme->preview.pageBg)),
              std::string::npos);
    EXPECT_NE(html.find(theme::ColorToCss(options.theme->tokens.comment)),
              std::string::npos);
}

TEST(PreviewHtml, InjectsHighlightScriptsOnlyForCode)
{
    PreviewHtmlOptions options;
    options.inlineScript = "var prismStub=1;";

    const std::string withCode = BuildPreviewHtml(
        "<pre><code class=\"language-cpp\">int main(){}</code></pre>", options);
    EXPECT_NE(withCode.find("window.Prism={manual:true};"), std::string::npos);
    EXPECT_NE(withCode.find("var prismStub=1;"), std::string::npos);
    EXPECT_NE(withCode.find("Prism.util.setLanguage"), std::string::npos);
    EXPECT_NE(withCode.find("getElementsByTagName('code')"), std::string::npos);
    EXPECT_LT(withCode.find("var prismStub=1;"),
              withCode.find("Prism.util.setLanguage"));

    const std::string withoutCode = BuildPreviewHtml("<p>plain</p>", options);
    EXPECT_EQ(withoutCode.find("<script>"), std::string::npos);
}

TEST(PreviewHtml, OmitsHighlightScriptsWithoutBundle)
{
    const std::string html =
        BuildPreviewHtml("<pre><code class=\"language-cpp\">x</code></pre>", {});
    EXPECT_EQ(html.find("<script>"), std::string::npos);
}

TEST(PreviewHtml, EscapesInlineScriptTerminators)
{
    PreviewHtmlOptions options;
    options.inlineScript = "var s='</script>';";
    const std::string html = BuildPreviewHtml(
        "<pre><code class=\"language-cpp\">x</code></pre>", options);

    EXPECT_EQ(html.find("var s='</script>'"), std::string::npos);
    EXPECT_NE(html.find("var s='<\\/script>'"), std::string::npos);
}

TEST(PreviewHtml, EmitsExtraCssAfterThemeCss)
{
    PreviewHtmlOptions options;
    options.extraCss = "body{outline:1px solid red;}";
    const std::string html = BuildPreviewHtml("body", options);

    const std::size_t themePos = html.find("#ffffff");
    const std::size_t extraPos = html.find("body{outline:1px solid red;}");
    ASSERT_NE(themePos, std::string::npos);
    ASSERT_NE(extraPos, std::string::npos);
    EXPECT_LT(themePos, extraPos);
}
