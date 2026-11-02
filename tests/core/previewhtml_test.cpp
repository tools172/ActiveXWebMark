#include <gtest/gtest.h>

#include <string>

#include "markdown/previewhtml.h"

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
