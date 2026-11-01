#include <gtest/gtest.h>

#include "highlight/stylemerge.h"

using namespace awm;

TEST(MergeStyleSpans, EmptySpansAreAllNormal)
{
    const auto styles = MergeStyleSpans(5, {});
    ASSERT_EQ(styles.size(), 5u);
    for (const MarkupStyle style : styles) {
        EXPECT_EQ(style, MarkupStyle::Normal);
    }
}

TEST(MergeStyleSpans, NonOverlappingSpans)
{
    const std::vector<StyleSpan> spans = {
        {0, 3, MarkupStyle::Heading},
        {3, 2, MarkupStyle::Code},
    };
    const auto styles = MergeStyleSpans(5, spans);
    EXPECT_EQ(styles[0], MarkupStyle::Heading);
    EXPECT_EQ(styles[1], MarkupStyle::Heading);
    EXPECT_EQ(styles[2], MarkupStyle::Heading);
    EXPECT_EQ(styles[3], MarkupStyle::Code);
    EXPECT_EQ(styles[4], MarkupStyle::Code);
}

TEST(MergeStyleSpans, OverlappingSpansInnerWins)
{
    // Heading covers [0,5); Strong covers [2,4). The later-starting (inner)
    // span must win on the overlap.
    const std::vector<StyleSpan> spans = {
        {0, 5, MarkupStyle::Heading},
        {2, 2, MarkupStyle::Strong},
    };
    const auto styles = MergeStyleSpans(5, spans);
    EXPECT_EQ(styles[0], MarkupStyle::Heading);
    EXPECT_EQ(styles[1], MarkupStyle::Heading);
    EXPECT_EQ(styles[2], MarkupStyle::Strong);
    EXPECT_EQ(styles[3], MarkupStyle::Strong);
    EXPECT_EQ(styles[4], MarkupStyle::Heading);
}

TEST(MergeStyleSpans, ClampsOutOfBoundsOffsets)
{
    // A span extending past the end must be clamped, never overflow.
    const std::vector<StyleSpan> spans = {{3, 100, MarkupStyle::Code}};
    const auto styles = MergeStyleSpans(5, spans);
    ASSERT_EQ(styles.size(), 5u);
    EXPECT_EQ(styles[0], MarkupStyle::Normal);
    EXPECT_EQ(styles[1], MarkupStyle::Normal);
    EXPECT_EQ(styles[2], MarkupStyle::Normal);
    EXPECT_EQ(styles[3], MarkupStyle::Code);
    EXPECT_EQ(styles[4], MarkupStyle::Code);
}
