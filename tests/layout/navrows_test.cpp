#include <gtest/gtest.h>

#include <vector>

#include "layout/navrows.h"

using namespace awm::ui;

TEST(NavRows, BuildsNestedHierarchy)
{
    const std::vector<NavRow> rows = buildNavRows({1, 2, 3, 2, 1, 2});

    ASSERT_EQ(rows.size(), 6u);

    EXPECT_EQ(rows[0].depth, 0);
    EXPECT_EQ(rows[0].parent, -1);
    EXPECT_TRUE(rows[0].hasChildren);

    EXPECT_EQ(rows[1].depth, 1);
    EXPECT_EQ(rows[1].parent, 0);
    EXPECT_TRUE(rows[1].hasChildren);

    EXPECT_EQ(rows[2].depth, 2);
    EXPECT_EQ(rows[2].parent, 1);
    EXPECT_FALSE(rows[2].hasChildren);

    EXPECT_EQ(rows[3].depth, 1);
    EXPECT_EQ(rows[3].parent, 0);

    EXPECT_EQ(rows[4].depth, 0);
    EXPECT_EQ(rows[4].parent, -1);
    EXPECT_TRUE(rows[4].hasChildren);

    EXPECT_EQ(rows[5].depth, 1);
    EXPECT_EQ(rows[5].parent, 4);
}

TEST(NavRows, SkippedLevelsNestUnderNearestAncestor)
{
    const std::vector<NavRow> rows = buildNavRows({1, 3, 4});
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[1].depth, 1);
    EXPECT_EQ(rows[1].parent, 0);
    EXPECT_EQ(rows[2].depth, 2);
    EXPECT_EQ(rows[2].parent, 1);
}

TEST(NavRows, StartingAtDeepLevelBecomesRoot)
{
    const std::vector<NavRow> rows = buildNavRows({3, 4});
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].depth, 0);
    EXPECT_EQ(rows[0].parent, -1);
    EXPECT_EQ(rows[1].parent, 0);
}

TEST(NavRows, EmptyInputYieldsNoRows)
{
    EXPECT_TRUE(buildNavRows({}).empty());
}
