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
    EXPECT_NE(result.bodyHtml.find("<pre><code"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("language-jsx"), std::string::npos);
}

TEST(CmarkEngine, RendersListInterruptingParagraph)
{
    // CommonMark: a list may interrupt a paragraph without a blank line.
    const RenderResult result = render("**bold**\n- item one\n- item two\n");
    EXPECT_NE(result.bodyHtml.find("<ul>"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<li>"), std::string::npos);
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
