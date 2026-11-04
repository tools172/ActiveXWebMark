#include <gtest/gtest.h>

#include "theme.h"

using namespace awm;

TEST(UiTheme, DefaultPaletteIsTheLightTheme)
{
    ui::Theme theme;
    theme.initialize(96);

    const ui::Palette& palette = theme.palette();
    EXPECT_EQ(palette.windowBg.r, 245);
    EXPECT_EQ(palette.windowBg.g, 246);
    EXPECT_EQ(palette.windowBg.b, 248);
    EXPECT_EQ(palette.surface.r, 255);
    EXPECT_EQ(palette.accent.r, 47);
    EXPECT_EQ(theme.px(10), 10);
}

TEST(UiTheme, SetPaletteReplacesColorsWithoutTouchingDpi)
{
    ui::Theme theme;
    theme.initialize(144);

    ui::Palette dark;
    dark.windowBg = ui::Color{10, 20, 30};
    dark.surface = ui::Color{40, 50, 60};
    dark.textPrimary = ui::Color{230, 231, 232};
    dark.accent = ui::Color{1, 2, 3};
    theme.setPalette(dark);

    EXPECT_EQ(theme.palette().windowBg.r, 10);
    EXPECT_EQ(theme.palette().surface.b, 60);
    EXPECT_EQ(theme.palette().textPrimary.r, 230);
    EXPECT_EQ(theme.palette().accent.g, 2);
    EXPECT_EQ(theme.px(96), 144);
}
