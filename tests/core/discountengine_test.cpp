#include <gtest/gtest.h>

#include "markdown/discountmarkdownengine.h"

using namespace awm;

namespace {

RenderResult render(const std::string& markdown)
{
    return DiscountMarkdownEngine{}.render(markdown, RenderOptions{});
}

} // namespace

TEST(DiscountEngine, RendersBasicMarkdown)
{
    const RenderResult result = render("# Hello\n\nSome *text*.\n");
    EXPECT_NE(result.bodyHtml.find("<h1"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<em>"), std::string::npos);
}

TEST(DiscountEngine, RendersCodeAndLists)
{
    const RenderResult result = render("- one\n- two\n\n`code`\n");
    EXPECT_NE(result.bodyHtml.find("<ul>"), std::string::npos);
    EXPECT_NE(result.bodyHtml.find("<code>"), std::string::npos);
}

TEST(DiscountEngine, ProducesOutline)
{
    const RenderResult result = render("# A\n## B\n");
    ASSERT_EQ(result.outline.size(), 2u);
    EXPECT_EQ(result.outline[0].text, "A");
    EXPECT_EQ(result.outline[1].text, "B");
}

TEST(DiscountEngine, HandlesEmptyInput)
{
    const RenderResult result = render("");
    EXPECT_TRUE(result.bodyHtml.empty());
    EXPECT_TRUE(result.outline.empty());
}
