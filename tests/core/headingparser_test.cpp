#include <gtest/gtest.h>

#include "document/headingparser.h"

using namespace awm;

TEST(HeadingParser, ExtractsAtxHeadingsWithLevelAndLine)
{
    const std::string markdown =
        "# Title\n"
        "intro\n"
        "## Section A\n"
        "### Sub\n"
        "#### Deep\n";

    const Outline outline = extractHeadings(markdown);

    ASSERT_EQ(outline.size(), 4u);
    EXPECT_EQ(outline[0].text, "Title");
    EXPECT_EQ(outline[0].level, 1);
    EXPECT_EQ(outline[0].line, 1);
    EXPECT_EQ(outline[1].text, "Section A");
    EXPECT_EQ(outline[1].line, 3);
    EXPECT_EQ(outline[2].level, 3);
    EXPECT_EQ(outline[3].level, 4);
}

TEST(HeadingParser, IgnoresNonHeadings)
{
    const std::string markdown =
        "#ok\n"          // no space after '#' -> not a heading
        "####### seven\n" // seven '#' -> not a heading
        "## real\n"
        "not a heading\n";

    const Outline outline = extractHeadings(markdown);
    ASSERT_EQ(outline.size(), 1u);
    EXPECT_EQ(outline[0].text, "real");
}

TEST(HeadingParser, StripsClosingHashes)
{
    const Outline outline = extractHeadings("# Hello ###\n");
    ASSERT_EQ(outline.size(), 1u);
    EXPECT_EQ(outline[0].text, "Hello");
}

TEST(SlugifyAnchor, LowercasesAndReplacesWhitespace)
{
    EXPECT_EQ(slugifyAnchor("Hello World"), "hello-world");
    EXPECT_EQ(slugifyAnchor("A  B"), "a-b"); // collapsed whitespace
}

TEST(SlugifyAnchor, PreservesUtf8Bytes)
{
    // Non-ASCII (UTF-8) bytes pass through verbatim.
    EXPECT_EQ(slugifyAnchor("你好 世界"), "你好-世界");
}

TEST(SlugifyAnchor, DropsPunctuation)
{
    EXPECT_EQ(slugifyAnchor("Hi (there)!"), "hi-there");
}

TEST(SlugifyAnchor, FallsBackForPunctuationOnlyHeading)
{
    EXPECT_EQ(slugifyAnchor("!!!"), "section");
}

TEST(MakeAnchorsUnique, SuffixesDuplicatesAndAvoidsLiteralCollisions)
{
    Outline outline(3);
    outline[0].text = "Foo";
    outline[0].anchor = "foo";
    outline[1].text = "Foo";
    outline[1].anchor = "foo";
    outline[2].text = "Foo 1";
    outline[2].anchor = "foo-1";

    makeAnchorsUnique(outline);

    EXPECT_EQ(outline[0].anchor, "foo");
    EXPECT_EQ(outline[1].anchor, "foo-2");
    EXPECT_EQ(outline[2].anchor, "foo-1");
}
