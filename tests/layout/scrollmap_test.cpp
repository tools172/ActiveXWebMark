#include <gtest/gtest.h>

#include <vector>

#include "layout/scrollmap.h"

using namespace awm::ui;

TEST(ScrollMap, EmptyMapUsesNeutralValues)
{
    const ScrollMap map;
    EXPECT_TRUE(map.empty());
    EXPECT_DOUBLE_EQ(map.yForLine(10.0), 0.0);
    EXPECT_DOUBLE_EQ(map.lineForY(100.0), 1.0);
}

TEST(ScrollMap, MapsWithinSingleBlock)
{
    ScrollMap map;
    map.rebuild({{10, 12, 100.0, 60.0}});

    EXPECT_DOUBLE_EQ(map.yForLine(10.0), 100.0);
    EXPECT_DOUBLE_EQ(map.yForLine(11.0), 120.0);
    EXPECT_DOUBLE_EQ(map.yForLine(12.0), 140.0);

    EXPECT_DOUBLE_EQ(map.lineForY(100.0), 10.0);
    EXPECT_DOUBLE_EQ(map.lineForY(130.0), 11.5);
    EXPECT_DOUBLE_EQ(map.lineForY(160.0), 12.0);
}

TEST(ScrollMap, InterpolatesSourceGapBetweenBlocks)
{
    ScrollMap map;
    map.rebuild({{1, 1, 0.0, 20.0}, {3, 3, 60.0, 20.0}});

    // Line 2 is between block ending at line 1 and block starting at line 3.
    EXPECT_DOUBLE_EQ(map.yForLine(2.0), 40.0);
    EXPECT_DOUBLE_EQ(map.lineForY(40.0), 2.0);
}

TEST(ScrollMap, InterpolatesPixelGapBetweenBlocks)
{
    ScrollMap map;
    map.rebuild({{1, 2, 0.0, 40.0}, {5, 6, 80.0, 40.0}});

    // y=60 is halfway between the first block bottom (40) and next top (80).
    EXPECT_DOUBLE_EQ(map.lineForY(60.0), 3.5);
    EXPECT_DOUBLE_EQ(map.yForLine(3.5), 60.0);
}

TEST(ScrollMap, PrefersDeepestNestedBlock)
{
    // Outer block covers lines 10..20 and 100px; inner block covers 12..13 and
    // 20px inside it.
    ScrollMap map;
    map.rebuild({{10, 20, 100.0, 100.0}, {12, 13, 130.0, 20.0}});

    EXPECT_DOUBLE_EQ(map.yForLine(12.0), 130.0);
    EXPECT_DOUBLE_EQ(map.lineForY(140.0), 13.0);
}

TEST(ScrollMap, ClampsBeforeFirstAndAfterLastBlock)
{
    ScrollMap map;
    map.rebuild({{5, 6, 50.0, 40.0}});

    EXPECT_DOUBLE_EQ(map.yForLine(1.0), 50.0);
    EXPECT_DOUBLE_EQ(map.yForLine(99.0), 90.0);
    EXPECT_DOUBLE_EQ(map.lineForY(0.0), 5.0);
    EXPECT_DOUBLE_EQ(map.lineForY(999.0), 6.0);
}

TEST(ScrollMap, DropsInvalidBlocks)
{
    ScrollMap map;
    map.rebuild({
        {0, 1, 0.0, 20.0},   // invalid start line
        {3, 2, 0.0, 20.0},   // end before start
        {4, 4, 0.0, 0.0},    // zero height
        {6, 6, 10.0, 20.0},  // valid
    });

    ASSERT_EQ(map.blocks().size(), 1u);
    EXPECT_EQ(map.blocks()[0].startLine, 6);
}

TEST(ScrollMap, SortsBlocksByDocumentPosition)
{
    ScrollMap map;
    map.rebuild({{5, 5, 80.0, 20.0}, {1, 1, 0.0, 20.0}});

    ASSERT_EQ(map.blocks().size(), 2u);
    EXPECT_EQ(map.blocks()[0].startLine, 1);
    EXPECT_EQ(map.blocks()[1].startLine, 5);
}
