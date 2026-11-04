#include <gtest/gtest.h>

#include <string>

#include "appsettings.h"
#include "../core/fake_settings.h"

using namespace awm;

TEST(AppSettings, LoadsStoredTheme)
{
    test::FakeSettings settings;
    settings.set("ui.theme", "dracula");

    const ViewSettings view = LoadViewSettings(settings);
    EXPECT_EQ(view.theme, "dracula");
}

TEST(AppSettings, MissingThemeUsesDefault)
{
    test::FakeSettings settings;
    const ViewSettings view = LoadViewSettings(settings);
    EXPECT_EQ(view.theme, "github-light");
}

TEST(AppSettings, InvalidThemeFallsBackToDefault)
{
    test::FakeSettings settings;
    settings.set("ui.theme", "not-a-theme");

    const ViewSettings view = LoadViewSettings(settings);
    EXPECT_EQ(view.theme, "github-light");
}

TEST(AppSettings, SavesSelectedTheme)
{
    test::FakeSettings settings;
    ViewSettings view;
    view.theme = "nord";

    SaveViewSettings(settings, view);
    EXPECT_EQ(settings.get("ui.theme"), "nord");
}

TEST(AppSettings, SavingAnInvalidThemeNormalizesIt)
{
    test::FakeSettings settings;
    ViewSettings view;
    view.theme = "bogus";

    SaveViewSettings(settings, view);
    EXPECT_EQ(settings.get("ui.theme"), "github-light");
}
