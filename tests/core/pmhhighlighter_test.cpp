#include <gtest/gtest.h>

#include "highlight/pmhhighlighter.h"

using namespace awm;

namespace {

bool hasStyle(const std::vector<StyleSpan>& spans, const std::string& text,
              std::size_t offset, MarkupStyle style)
{
    for (const StyleSpan& span : spans) {
        if (span.offset == offset && span.style == style &&
            text.substr(span.offset, span.length) == text.substr(offset, span.length)) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(PmhHighlighter, DetectsHeading)
{
    const std::string markdown = "# Title\n";
    const auto spans = PmhSyntaxHighlighter{}.highlight(markdown);

    bool found = false;
    for (const StyleSpan& span : spans) {
        if (span.style == MarkupStyle::Heading && span.offset == 0) {
            found = true;
            EXPECT_EQ(markdown.substr(span.offset, span.length), "# Title");
        }
    }
    EXPECT_TRUE(found);
}

TEST(PmhHighlighter, DetectsEmphasisAndCode)
{
    const std::string markdown = "a *b* c `d`\n";
    const auto spans = PmhSyntaxHighlighter{}.highlight(markdown);

    bool em = false, code = false;
    for (const StyleSpan& span : spans) {
        if (span.style == MarkupStyle::Emphasis) {
            em = true;
            EXPECT_EQ(markdown.substr(span.offset, span.length), "*b*");
        }
        if (span.style == MarkupStyle::Code) {
            code = true;
            EXPECT_EQ(markdown.substr(span.offset, span.length), "`d`");
        }
    }
    EXPECT_TRUE(em);
    EXPECT_TRUE(code);
}

TEST(PmhHighlighter, OffsetsAreByteOffsetsWithUtf8)
{
    // "你" is 3 bytes in UTF-8; offsets must therefore be byte (not codepoint)
    // offsets so that they line up with the source string.
    const std::string markdown = "你好 *世界*\n"; // '你','好' = 6 bytes, then ' ' = byte 6
    const auto spans = PmhSyntaxHighlighter{}.highlight(markdown);

    bool found = false;
    for (const StyleSpan& span : spans) {
        if (span.style == MarkupStyle::Emphasis) {
            found = true;
            EXPECT_EQ(markdown.substr(span.offset, span.length), "*世界*");
            EXPECT_EQ(span.offset, 7u); // 6 (你好) + 1 (space)
        }
    }
    EXPECT_TRUE(found);
}

TEST(PmhHighlighter, SpansStayWithinDocumentBounds)
{
    const std::string markdown =
        "# **Bold 标题** and *italic* `code`\n"
        "- list item\n"
        "## 二级标题\n";
    const auto spans = PmhSyntaxHighlighter{}.highlight(markdown);

    for (const StyleSpan& span : spans) {
        EXPECT_LE(span.offset + span.length, markdown.size())
            << "span offset=" << span.offset << " length=" << span.length;
    }
}
