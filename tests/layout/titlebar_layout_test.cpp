#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "layout/titlebar_layout.h"

using namespace awm::ui;

namespace {

TitleBarMetrics defaultMetrics()
{
    return TitleBarMetrics{};
}

TitleBarLayout wideLayout()
{
    return layoutTitleBar(RectI{0, 0, 1000, 36}, defaultMetrics(),
                          std::vector<int>{60, 60, 60, 60});
}

} // namespace

// ---------------------------------------------------------------------------
// Menu label parsing
// ---------------------------------------------------------------------------

TEST(TitleBarLayout, ParsesChineseMnemonicGroup)
{
    const MenuLabel label = parseMenuLabel(L"文件(&F)");
    EXPECT_EQ(label.text, L"文件");
    EXPECT_EQ(label.mnemonic, L'F');
}

TEST(TitleBarLayout, ParsesEachTopLevelLabel)
{
    const std::vector<std::pair<std::wstring, std::wstring>> cases = {
        {L"编辑(&E)", L"编辑"},
        {L"视图(&V)", L"视图"},
        {L"帮助(&H)", L"帮助"},
    };
    for (const auto& [raw, expected] : cases) {
        const MenuLabel label = parseMenuLabel(raw);
        EXPECT_EQ(label.text, expected);
        ASSERT_NE(label.mnemonic, 0);
    }
    EXPECT_EQ(parseMenuLabel(L"编辑(&E)").mnemonic, L'E');
    EXPECT_EQ(parseMenuLabel(L"视图(&V)").mnemonic, L'V');
    EXPECT_EQ(parseMenuLabel(L"帮助(&H)").mnemonic, L'H');
}

TEST(TitleBarLayout, ParsesAmpersandMnemonic)
{
    const MenuLabel label = parseMenuLabel(L"&File");
    EXPECT_EQ(label.text, L"File");
    EXPECT_EQ(label.mnemonic, L'F');
}

TEST(TitleBarLayout, ParsesNoMnemonic)
{
    const MenuLabel label = parseMenuLabel(L"Plain");
    EXPECT_EQ(label.text, L"Plain");
    EXPECT_EQ(label.mnemonic, 0);
}

TEST(TitleBarLayout, EscapedAmpersandStaysLiteral)
{
    const MenuLabel label = parseMenuLabel(L"A && B");
    EXPECT_EQ(label.text, L"A & B");
    EXPECT_EQ(label.mnemonic, 0);
}

TEST(TitleBarLayout, DropsAcceleratorColumn)
{
    const MenuLabel label = parseMenuLabel(L"打开(&O)...\tCtrl+O");
    EXPECT_EQ(label.text, L"打开(O)...");
    EXPECT_EQ(label.mnemonic, L'O');
}

TEST(TitleBarLayout, TrailingMnemonicGroupWinsOverInlineMnemonic)
{
    const MenuLabel label = parseMenuLabel(L"&Save (&S)");
    EXPECT_EQ(label.text, L"Save");
    EXPECT_EQ(label.mnemonic, L'S');
}

TEST(TitleBarLayout, TrimsWhitespaceAndHandlesEmptyText)
{
    EXPECT_EQ(parseMenuLabel(L"  文件 (&F)  ").text, L"文件");
    const MenuLabel empty = parseMenuLabel(L"(&X)");
    EXPECT_TRUE(empty.text.empty());
    EXPECT_EQ(empty.mnemonic, L'X');
    EXPECT_TRUE(parseMenuLabel(L"").text.empty());
}

TEST(TitleBarLayout, LoneTrailingAmpersandIsDropped)
{
    const MenuLabel label = parseMenuLabel(L"Help&");
    EXPECT_EQ(label.text, L"Help");
    EXPECT_EQ(label.mnemonic, 0);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

TEST(TitleBarLayout, WideLayoutPlacesEveryPart)
{
    const TitleBarLayout layout = wideLayout();

    EXPECT_EQ(layout.bar.x, 0);
    EXPECT_EQ(layout.bar.w, 1000);
    EXPECT_EQ(layout.bar.h, 36);

    EXPECT_EQ(layout.icon.x, 10);
    EXPECT_EQ(layout.icon.y, 10);
    EXPECT_EQ(layout.icon.w, 16);
    EXPECT_EQ(layout.icon.h, 16);

    ASSERT_EQ(layout.menus.size(), 4u);
    EXPECT_EQ(layout.menus[0].x, 36);
    EXPECT_EQ(layout.menus[0].w, 60);
    EXPECT_EQ(layout.menus[1].x, 98);
    EXPECT_EQ(layout.menus[2].x, 160);
    EXPECT_EQ(layout.menus[3].x, 222);
    for (const RectI& menu : layout.menus) {
        EXPECT_EQ(menu.y, 0);
        EXPECT_EQ(menu.h, 36);
    }

    // 222 + 60 + gap 2 = 284, plus the icon margin used as a separator.
    EXPECT_EQ(layout.title.x, 294);
    EXPECT_EQ(layout.title.right(), 852);
    EXPECT_EQ(layout.title.w, 558);
    EXPECT_EQ(layout.title.h, 36);

    EXPECT_EQ(layout.closeButton.x, 954);
    EXPECT_EQ(layout.maximizeButton.x, 908);
    EXPECT_EQ(layout.minimizeButton.x, 862);
    EXPECT_EQ(layout.closeButton.w, 46);
    EXPECT_EQ(layout.maximizeButton.w, 46);
    EXPECT_EQ(layout.minimizeButton.w, 46);
    for (const RectI* button :
         {&layout.minimizeButton, &layout.maximizeButton, &layout.closeButton}) {
        EXPECT_EQ(button->y, 0);
        EXPECT_EQ(button->h, 36);
    }
}

TEST(TitleBarLayout, WideLayoutPartsDoNotOverlap)
{
    const TitleBarLayout layout = wideLayout();

    EXPECT_LE(layout.icon.right(), layout.menus.front().x);
    for (std::size_t i = 1; i < layout.menus.size(); ++i) {
        EXPECT_LT(layout.menus[i - 1].right(), layout.menus[i].x);
    }
    EXPECT_LE(layout.menus.back().right(), layout.title.x);
    EXPECT_LE(layout.title.right(), layout.minimizeButton.x);
    EXPECT_LE(layout.minimizeButton.right(), layout.maximizeButton.x);
    EXPECT_LE(layout.maximizeButton.right(), layout.closeButton.x);
    EXPECT_EQ(layout.closeButton.right(), layout.bar.right());
}

TEST(TitleBarLayout, EmptyMenuListLeavesTitleAfterIcon)
{
    const TitleBarLayout layout =
        layoutTitleBar(RectI{0, 0, 600, 36}, defaultMetrics(), {});
    EXPECT_TRUE(layout.menus.empty());
    EXPECT_EQ(layout.icon.right(), 26);
    EXPECT_EQ(layout.title.x, 46);
    EXPECT_GE(layout.title.w, 0);
}

TEST(TitleBarLayout, MenuWidthsTreatNegativeAsZero)
{
    const TitleBarLayout layout =
        layoutTitleBar(RectI{0, 0, 600, 36}, defaultMetrics(), {-10, 40});
    ASSERT_EQ(layout.menus.size(), 2u);
    EXPECT_EQ(layout.menus[0].w, 0);
    EXPECT_EQ(layout.menus[1].x, 38); // gap only between entries
    EXPECT_EQ(layout.menus[1].w, 40);
}

TEST(TitleBarLayout, NarrowBarCollapsesTitleButKeepsButtons)
{
    const TitleBarLayout layout =
        layoutTitleBar(RectI{0, 0, 120, 36}, defaultMetrics(),
                       std::vector<int>{60, 60, 60, 60});
    EXPECT_EQ(layout.title.w, 0);
    EXPECT_EQ(layout.title.x, 120);
    EXPECT_EQ(layout.closeButton.x, 74);
    EXPECT_EQ(layout.maximizeButton.x, 28);
    EXPECT_EQ(layout.minimizeButton.x, -18);
}

TEST(TitleBarLayout, ZeroAndNegativeSizeProduceEmptyRects)
{
    for (const RectI& bar : {RectI{0, 0, 0, 36}, RectI{0, 0, 200, 0},
                             RectI{0, 0, -10, -10}}) {
        const TitleBarLayout layout =
            layoutTitleBar(bar, defaultMetrics(), std::vector<int>{60});
        EXPECT_TRUE(layout.icon.empty());
        ASSERT_EQ(layout.menus.size(), 1u);
        EXPECT_TRUE(layout.menus[0].empty());
        EXPECT_TRUE(layout.title.empty());
        EXPECT_TRUE(layout.minimizeButton.empty());
        EXPECT_TRUE(layout.maximizeButton.empty());
        EXPECT_TRUE(layout.closeButton.empty());
    }
}

TEST(TitleBarLayout, IconSizeIsClampedToBarHeight)
{
    TitleBarMetrics metrics = defaultMetrics();
    metrics.iconSize = 100;
    const TitleBarLayout layout =
        layoutTitleBar(RectI{0, 0, 600, 20}, metrics, {});
    EXPECT_EQ(layout.icon.w, 20);
    EXPECT_EQ(layout.icon.h, 20);
    EXPECT_EQ(layout.icon.y, 0);
}

TEST(TitleBarLayout, LayoutKeepsInvariantsAcrossWidths)
{
    const std::vector<int> widths{0, 32, 40, 40, 40};
    for (int width = 1; width <= 1200; width += 37) {
        const TitleBarLayout layout =
            layoutTitleBar(RectI{0, 0, width, 36}, defaultMetrics(), widths);
        EXPECT_GE(layout.title.w, 0);
        EXPECT_LE(layout.title.x, layout.title.right());
        for (const RectI& menu : layout.menus) {
            EXPECT_GE(menu.w, 0);
            EXPECT_EQ(menu.h, 36);
        }
        for (const RectI* button :
             {&layout.minimizeButton, &layout.maximizeButton, &layout.closeButton}) {
            EXPECT_GE(button->w, 0);
            EXPECT_EQ(button->h, 36);
        }
    }
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

TEST(TitleBarHitTest, FindsMenusAndReportsIndex)
{
    const TitleBarLayout layout = wideLayout();
    for (int i = 0; i < 4; ++i) {
        const RectI& menu = layout.menus[static_cast<std::size_t>(i)];
        int index = -1;
        const TitleBarHit hit =
            hitTestTitleBar(layout, menu.x + menu.w / 2, menu.y + menu.h / 2, &index);
        EXPECT_EQ(hit, TitleBarHit::Menu);
        EXPECT_EQ(index, i);
    }
}

TEST(TitleBarHitTest, FindsWindowButtons)
{
    const TitleBarLayout layout = wideLayout();
    const auto center = [](const RectI& rect) {
        return std::pair<int, int>{rect.x + rect.w / 2, rect.y + rect.h / 2};
    };

    EXPECT_EQ(hitTestTitleBar(layout, center(layout.minimizeButton).first,
                              center(layout.minimizeButton).second),
              TitleBarHit::Minimize);
    EXPECT_EQ(hitTestTitleBar(layout, center(layout.maximizeButton).first,
                              center(layout.maximizeButton).second),
              TitleBarHit::Maximize);
    EXPECT_EQ(hitTestTitleBar(layout, center(layout.closeButton).first,
                              center(layout.closeButton).second),
              TitleBarHit::Close);
}

TEST(TitleBarHitTest, FindsIconAndCaption)
{
    const TitleBarLayout layout = wideLayout();
    EXPECT_EQ(hitTestTitleBar(layout, layout.icon.x + 1, layout.icon.y + 1),
              TitleBarHit::Icon);

    // Between the icon and the first menu label.
    EXPECT_EQ(hitTestTitleBar(layout, 30, 18), TitleBarHit::Caption);
    // Centered in the free title area.
    EXPECT_EQ(hitTestTitleBar(layout, layout.title.x + 10, 18), TitleBarHit::Caption);
}

TEST(TitleBarHitTest, OutsideBarReturnsNone)
{
    const TitleBarLayout layout = wideLayout();
    EXPECT_EQ(hitTestTitleBar(layout, -1, 18), TitleBarHit::None);
    EXPECT_EQ(hitTestTitleBar(layout, 1000, 18), TitleBarHit::None);
    EXPECT_EQ(hitTestTitleBar(layout, 10, -1), TitleBarHit::None);
    EXPECT_EQ(hitTestTitleBar(layout, 10, 36), TitleBarHit::None);
}

TEST(TitleBarHitTest, RightAndBottomEdgesAreExclusive)
{
    const TitleBarLayout layout = wideLayout();
    // 文件 label spans [36, 96); 96 belongs to the gap before 编辑.
    EXPECT_EQ(hitTestTitleBar(layout, 36, 0), TitleBarHit::Menu);
    EXPECT_EQ(hitTestTitleBar(layout, 95, 35), TitleBarHit::Menu);
    int index = -2;
    EXPECT_EQ(hitTestTitleBar(layout, 96, 18, &index), TitleBarHit::Caption);
    EXPECT_EQ(index, -1);
    // The bar's bottom edge (y == 36) is outside.
    EXPECT_EQ(hitTestTitleBar(layout, 50, 36), TitleBarHit::None);
}

TEST(TitleBarHitTest, ButtonsWinWhenNarrowBarOverlapsMenus)
{
    const TitleBarLayout layout =
        layoutTitleBar(RectI{0, 0, 120, 36}, defaultMetrics(),
                       std::vector<int>{60, 60, 60, 60});
    // x=50 is inside menu 0 but also inside the maximize button.
    int index = -1;
    EXPECT_EQ(hitTestTitleBar(layout, 50, 18, &index), TitleBarHit::Maximize);
    EXPECT_EQ(index, -1);
}

TEST(TitleBarHitTest, MenuIndexIsResetWhenNotAMenu)
{
    const TitleBarLayout layout = wideLayout();
    int index = 7;
    EXPECT_EQ(hitTestTitleBar(layout, layout.closeButton.x + 2, 18, &index),
              TitleBarHit::Close);
    EXPECT_EQ(index, -1);
}
