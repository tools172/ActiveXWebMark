#include <gtest/gtest.h>

#include <vector>

#include "layout/geometry.h"

using namespace awm::ui;

TEST(Geometry, SplitAreaHorizontal)
{
    const SplitLayout layout =
        splitArea(RectI{0, 0, 1000, 600}, Orientation::Horizontal, 400, 6, 100, 100);

    EXPECT_EQ(layout.first.x, 0);
    EXPECT_EQ(layout.first.w, 400);
    EXPECT_EQ(layout.divider.x, 400);
    EXPECT_EQ(layout.divider.w, 6);
    EXPECT_EQ(layout.second.x, 406);
    EXPECT_EQ(layout.second.w, 594);
    EXPECT_EQ(layout.second.h, 600);
}

TEST(Geometry, SplitAreaClampsBothMinimums)
{
    const SplitLayout wide =
        splitArea(RectI{0, 0, 1000, 600}, Orientation::Horizontal, 950, 6, 100, 100);
    EXPECT_EQ(wide.first.w, 894);
    EXPECT_EQ(wide.second.w, 100);

    const SplitLayout narrow =
        splitArea(RectI{0, 0, 1000, 600}, Orientation::Horizontal, 10, 6, 100, 100);
    EXPECT_EQ(narrow.first.w, 100);
    EXPECT_EQ(narrow.second.w, 894);
}

TEST(Geometry, SplitAreaFallsBackToHalfWhenTooSmall)
{
    const SplitLayout layout =
        splitArea(RectI{0, 0, 150, 200}, Orientation::Horizontal, 100, 6, 100, 100);
    EXPECT_EQ(layout.first.w, 72);
    EXPECT_EQ(layout.second.w, 72);
}

TEST(Geometry, SplitAreaVertical)
{
    const SplitLayout layout =
        splitArea(RectI{10, 20, 800, 500}, Orientation::Vertical, 200, 6, 50, 50);
    EXPECT_EQ(layout.first.y, 20);
    EXPECT_EQ(layout.first.h, 200);
    EXPECT_EQ(layout.divider.y, 220);
    EXPECT_EQ(layout.divider.h, 6);
    EXPECT_EQ(layout.second.y, 226);
    EXPECT_EQ(layout.second.h, 294);
}

TEST(Geometry, LayoutWithNavOnRight)
{
    const NavLayout layout = layoutWithNav(RectI{0, 0, 1000, 600}, true, 260, 6, 160, 480, 300);
    EXPECT_TRUE(layout.dividerVisible);
    EXPECT_EQ(layout.content.w, 734);
    EXPECT_EQ(layout.divider.x, 734);
    EXPECT_EQ(layout.divider.w, 6);
    EXPECT_EQ(layout.nav.x, 740);
    EXPECT_EQ(layout.nav.w, 260);
    EXPECT_EQ(layout.nav.h, 600);
}

TEST(Geometry, LayoutWithNavRespectsMinimumsAndMaximum)
{
    const NavLayout tooSmall = layoutWithNav(RectI{0, 0, 1000, 600}, true, 10, 6, 160, 480, 300);
    EXPECT_EQ(tooSmall.nav.w, 160);

    const NavLayout tooLarge = layoutWithNav(RectI{0, 0, 1000, 600}, true, 900, 6, 160, 480, 300);
    EXPECT_EQ(tooLarge.nav.w, 480);
}

TEST(Geometry, LayoutWithNavCollapsesWhenWindowIsTooSmall)
{
    const NavLayout layout = layoutWithNav(RectI{0, 0, 200, 600}, true, 260, 6, 160, 480, 300);
    EXPECT_FALSE(layout.dividerVisible);
    EXPECT_EQ(layout.content.w, 200);
    EXPECT_EQ(layout.nav.w, 0);
}

TEST(Geometry, LayoutWithNavHiddenFillsClient)
{
    const NavLayout layout = layoutWithNav(RectI{0, 0, 800, 600}, false, 260, 6, 160, 480, 300);
    EXPECT_FALSE(layout.dividerVisible);
    EXPECT_EQ(layout.content.w, 800);
}

TEST(Geometry, LayoutTabsFitsVisibleTabs)
{
    const TabStripLayout layout =
        layoutTabs(RectI{0, 0, 500, 38}, {100, 100, 100}, 4, 8, 16, 0);

    ASSERT_EQ(layout.slots.size(), 3u);
    EXPECT_EQ(layout.firstVisible, 0);
    EXPECT_EQ(layout.slots[0].rect.x, 8);
    EXPECT_EQ(layout.slots[1].rect.x, 112);
    EXPECT_EQ(layout.slots[2].rect.x, 216);
    EXPECT_EQ(layout.slots[0].closeRect.x, 8 + 100 - 4 - 16);
    EXPECT_EQ(layout.slots[0].closeRect.y, (38 - 16) / 2);
    EXPECT_TRUE(layout.slots[2].visible);
}

TEST(Geometry, LayoutTabsMarksOverflowInvisible)
{
    const TabStripLayout layout =
        layoutTabs(RectI{0, 0, 500, 38}, {300, 300}, 4, 8, 16, 0);

    ASSERT_EQ(layout.slots.size(), 2u);
    EXPECT_TRUE(layout.slots[0].visible);
    EXPECT_FALSE(layout.slots[1].visible);
    EXPECT_TRUE(layout.slots[1].rect.empty());
}

TEST(Geometry, LayoutTabsHonoursFirstVisible)
{
    const TabStripLayout layout =
        layoutTabs(RectI{0, 0, 500, 38}, {100, 100, 100}, 4, 8, 16, 2);

    ASSERT_EQ(layout.slots.size(), 3u);
    EXPECT_FALSE(layout.slots[0].visible);
    EXPECT_FALSE(layout.slots[1].visible);
    EXPECT_TRUE(layout.slots[2].visible);
    EXPECT_EQ(layout.slots[2].rect.x, 8);
}

TEST(Geometry, ClampTabScroll)
{
    EXPECT_EQ(clampTabScroll(5, {}), 0);
    EXPECT_EQ(clampTabScroll(-2, {10, 10, 10}), 0);
    EXPECT_EQ(clampTabScroll(9, {10, 10, 10}), 2);
}

TEST(Geometry, ScrollToShowActiveTab)
{
    EXPECT_EQ(scrollToShow(1, 0, 500, {100, 100, 100}, 4, 8), 0);
    EXPECT_EQ(scrollToShow(2, 0, 500, {200, 200, 200}, 4, 8), 1);
    EXPECT_EQ(scrollToShow(0, 2, 500, {100, 100, 100}, 4, 8), 0);
}
