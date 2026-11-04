#include <gtest/gtest.h>

#include "theme/theme_catalog.h"
#include "themeapply.h"

using namespace awm;

TEST(ThemeApply, GithubLightMatchesTheBuiltInLightPalette)
{
    const ui::Palette palette =
        MakePalette(theme::ResolveTheme(theme::kDefaultThemeId));
    const ui::Palette defaults;

    EXPECT_EQ(palette.windowBg.r, defaults.windowBg.r);
    EXPECT_EQ(palette.windowBg.g, defaults.windowBg.g);
    EXPECT_EQ(palette.windowBg.b, defaults.windowBg.b);
    EXPECT_EQ(palette.surface.r, defaults.surface.r);
    EXPECT_EQ(palette.navBg.r, defaults.navBg.r);
    EXPECT_EQ(palette.textPrimary.r, defaults.textPrimary.r);
    EXPECT_EQ(palette.accent.r, defaults.accent.r);
    EXPECT_EQ(palette.gutterBg.r, defaults.gutterBg.r);
}

TEST(ThemeApply, DarkThemesUseDarkSurfacesAndLightText)
{
    for (const char* id : {"one-dark", "dracula", "nord"}) {
        const theme::ThemeDefinition& definition = theme::ResolveTheme(id);
        ASSERT_TRUE(definition.dark) << id;
        const ui::Palette palette = MakePalette(definition);

        EXPECT_LT(palette.surface.r, 80) << id;
        EXPECT_LT(palette.surface.g, 80) << id;
        EXPECT_LT(palette.surface.b, 90) << id;
        EXPECT_GT(palette.textPrimary.r, 150) << id;
        EXPECT_GT(palette.textPrimary.g, 150) << id;
        EXPECT_GT(palette.textPrimary.b, 150) << id;
    }
}

TEST(ThemeApply, UsesThemeAccentForDirtyMarker)
{
    const theme::ThemeDefinition& dracula = theme::ResolveTheme("dracula");
    const ui::Palette palette = MakePalette(dracula);
    EXPECT_EQ(palette.dirtyDot.r, dracula.chrome.dirtyDot.r);
    EXPECT_EQ(palette.dirtyDot.g, dracula.chrome.dirtyDot.g);
    EXPECT_EQ(palette.dirtyDot.b, dracula.chrome.dirtyDot.b);
}
