#include <gtest/gtest.h>

#include "commands.h"
#include "theme/theme_catalog.h"
#include "themeapply.h"

using namespace awm;

TEST(ThemeApply, CatalogFitsCommandIdRange)
{
    EXPECT_LE(theme::AllThemes().size(), static_cast<std::size_t>(cmd::kViewThemeMax));
}

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

TEST(ThemeApply, DetectsDarkPalettesForNativeControls)
{
    EXPECT_FALSE(IsDarkPalette(MakePalette(theme::ResolveTheme("github-light"))));
    EXPECT_FALSE(IsDarkPalette(MakePalette(theme::ResolveTheme("ulysses"))));
    EXPECT_FALSE(IsDarkPalette(MakePalette(theme::ResolveTheme("solarized-light"))));
    EXPECT_TRUE(IsDarkPalette(MakePalette(theme::ResolveTheme("one-dark"))));
    EXPECT_TRUE(IsDarkPalette(MakePalette(theme::ResolveTheme("dracula"))));
    EXPECT_TRUE(IsDarkPalette(MakePalette(theme::ResolveTheme("nord"))));
}

TEST(ThemeApply, EveryThemeMatchesItsDarkFlag)
{
    for (const theme::ThemeDefinition& definition : theme::AllThemes()) {
        EXPECT_EQ(IsDarkPalette(MakePalette(definition)), definition.dark)
            << definition.id;
    }
}

TEST(ThemeApply, ScrollbarColorsFollowEachTheme)
{
    const ui::ScrollbarColors oneDark =
        MakeScrollbarColors(theme::ResolveTheme("one-dark"));
    EXPECT_EQ(oneDark.track, RGB(40, 44, 52));
    EXPECT_EQ(oneDark.thumb, RGB(0x49, 0x4E, 0x58));

    const ui::ScrollbarColors dracula =
        MakeScrollbarColors(theme::ResolveTheme("dracula"));
    EXPECT_EQ(dracula.track, RGB(40, 42, 54));
    EXPECT_EQ(dracula.thumb, RGB(0x62, 0x64, 0x6B));

    const ui::ScrollbarColors github =
        MakeScrollbarColors(theme::ResolveTheme("github-light"));
    EXPECT_EQ(github.track, RGB(255, 255, 255));
    EXPECT_EQ(github.thumb, RGB(0xC0, 0xC1, 0xC3));

    const ui::Palette palette = MakePalette(theme::ResolveTheme("one-dark"));
    const ui::ScrollbarColors fromPalette = MakeScrollbarColors(palette);
    EXPECT_EQ(fromPalette.track, ui::ToColorRef(palette.surface));
    EXPECT_EQ(fromPalette.arrow, ui::ToColorRef(palette.textSecondary));
}

TEST(ThemeApply, NavigationScrollbarUsesSidebarBackground)
{
    const ui::Palette palette =
        MakePalette(theme::ResolveTheme(theme::kDefaultThemeId));
    const ui::ScrollbarColors nav = MakeNavScrollbarColors(palette);
    EXPECT_EQ(nav.track, ui::ToColorRef(palette.windowBg));
    EXPECT_EQ(nav.arrow, ui::ToColorRef(palette.textSecondary));
}
