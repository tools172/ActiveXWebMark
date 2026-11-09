#include <gtest/gtest.h>

#include "markdown/cmarkengine.h"

using namespace awm;

namespace {

RenderResult render(const std::string& markdown)
{
    return CmarkEngine{}.render(markdown, RenderOptions{});
}

} // namespace

TEST(CmarkEngine, RendersFencedCodeBlock)
{
    const RenderResult result = render("```jsx\nconst a = 1;\n```\n");
    // SOURCEPOS adds attributes to <pre>, so do not require "<pre><code".
    EXPECT_NE(result.bodyHtml.find("<pre"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<code"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("language-jsx"), std::string::npos);
}

TEST(CmarkEngine, RendersListInterruptingParagraph)
{
    // CommonMark: a list may interrupt a paragraph without a blank line.
    const RenderResult result = render("**bold**\n- item one\n- item two\n");
    EXPECT_NE(result.bodyHtml.find("<ul"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<li"), std::string::npos);
}

TEST(CmarkEngine, RendersGfmTable)
{
    const RenderResult result = render("| Name | Value |\n"
                                       "|:-----|------:|\n"
                                       "| **a** | `b` |\n");
    EXPECT_NE(result.bodyHtml.find("<table"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<thead>"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<tbody>"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<th align=\"left\""), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<td align=\"right\""), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<strong>a</strong>"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<code>b</code>"), std::string::npos);
}

TEST(CmarkEngine, TableCarriesSourcePositions)
{
    const RenderResult result = render("| A | B |\n|---|---|\n| 1 | 2 |\n");
    EXPECT_NE(result.bodyHtml.find("<table data-sourcepos=\""),
              std::string::npos);
}

TEST(CmarkEngine, PercentEncodesNonAsciiAnchorHref)
{
    const RenderResult result = render("[跳转](#中文标题)\n\n# 中文标题\n");
    // The heading keeps the Unicode anchor, while cmark percent-encodes the
    // link fragment; the preview link handler decodes it before lookup.
    EXPECT_NE(result.bodyHtml.find("id=\"中文标题\""), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("href=\"#%E4%B8%AD%E6%96%87%E6%A0%87%E9%A2%98\""),
              std::string::npos);
}

TEST(CmarkEngine, ProducesOutline)
{
    const RenderResult result = render("# Title\n## Section\n### Sub\n");
    ASSERT_EQ(result.outline.size(), 3u);
    EXPECT_EQ(result.outline[0].text, "Title");
    EXPECT_EQ(result.outline[0].level, 1);
    EXPECT_EQ(result.outline[1].text, "Section");
    EXPECT_EQ(result.outline[2].level, 3);
}

TEST(CmarkEngine, HeadingIdsMatchOutlineAnchors)
{
    const RenderResult result = render("# Hello World\n## 中文标题\n");
    ASSERT_EQ(result.outline.size(), 2u);
    for (const OutlineItem& item : result.outline) {
        const std::string expected = "id=\"" + item.anchor + "\"";
        EXPECT_NE(result.bodyHtml.find(expected), std::string::npos)
            << "heading \"" << item.text << "\" should emit " << expected;
    }
}

TEST(CmarkEngine, HtmlCarriesSourcePositions)
{
    const RenderResult result = render("# Title\n\nParagraph\n");
    EXPECT_NE(result.bodyHtml.find("data-sourcepos=\""), std::string::npos);
    ASSERT_EQ(result.outline.size(), 1u);
    EXPECT_NE(result.bodyHtml.find("id=\"title\""), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<h1 id=\"title\" data-sourcepos=\""),
              std::string::npos);
}

TEST(CmarkEngine, DuplicateHeadingsGetUniqueAnchors)
{
    const RenderResult result = render("# Same\n\n## Same\n\n### Same\n");
    ASSERT_EQ(result.outline.size(), 3u);
    EXPECT_EQ(result.outline[0].anchor, "same");
    EXPECT_EQ(result.outline[1].anchor, "same-1");
    EXPECT_EQ(result.outline[2].anchor, "same-2");

    for (const OutlineItem& item : result.outline) {
        const std::string expected = "id=\"" + item.anchor + "\"";
        EXPECT_NE(result.bodyHtml.find(expected), std::string::npos);
    }
}
