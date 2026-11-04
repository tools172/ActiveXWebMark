#include <gtest/gtest.h>

#include <set>
#include <string>

#include "markdown/previewcss.h"
#include "theme/theme_catalog.h"

using namespace awm;

TEST(ThemeCatalog, HasSixUniqueThemesAndGithubDefault)
{
    const auto& themes = theme::AllThemes();
    ASSERT_EQ(themes.size(), 6u);

    std::set<std::string> ids;
    for (const theme::ThemeDefinition& entry : themes) {
        EXPECT_FALSE(std::string(entry.id).empty());
        EXPECT_TRUE(ids.insert(entry.id).second) << "duplicate id " << entry.id;
    }

    EXPECT_EQ(std::string(theme::DefaultTheme().id), "github-light");
    EXPECT_EQ(std::string(theme::kDefaultThemeId), "github-light");
}

TEST(ThemeCatalog, ResolvesUnknownIdsToDefault)
{
    EXPECT_EQ(theme::ResolveTheme("does-not-exist").id, theme::DefaultTheme().id);
    EXPECT_EQ(theme::FindTheme("does-not-exist"), nullptr);
    EXPECT_EQ(theme::IndexOfTheme("does-not-exist"), -1);
    EXPECT_EQ(theme::IndexOfTheme("dracula"), 4);
}

TEST(ThemeCatalog, PreviewCssIsThemedAndIe11Safe)
{
    for (const theme::ThemeDefinition& entry : theme::AllThemes()) {
        const std::string css = BuildPreviewCss(entry);
        EXPECT_NE(css.find(theme::ColorToCss(entry.preview.pageBg)), std::string::npos)
            << entry.id;
        EXPECT_NE(css.find(".token.comment"), std::string::npos) << entry.id;
        EXPECT_NE(css.find(".awm-find"), std::string::npos) << entry.id;
        EXPECT_NE(css.find("pre code"), std::string::npos) << entry.id;
        EXPECT_NE(css.find("scrollbar-face-color:"), std::string::npos) << entry.id;
        EXPECT_NE(css.find("scrollbar-track-color:"), std::string::npos) << entry.id;
        EXPECT_EQ(css.find("var("), std::string::npos) << entry.id;
        EXPECT_EQ(css.find(":root"), std::string::npos) << entry.id;
        EXPECT_EQ(css.find("--"), std::string::npos) << entry.id;
    }
}

TEST(ThemeCatalog, PrimaryTextHasReadableContrast)
{
    for (const theme::ThemeDefinition& entry : theme::AllThemes()) {
        EXPECT_GE(theme::ContrastRatio(entry.preview.pageText, entry.preview.pageBg), 4.0)
            << entry.id << " preview";
        EXPECT_GE(theme::ContrastRatio(entry.editor.text, entry.editor.background), 4.0)
            << entry.id << " editor";
        EXPECT_GE(theme::ContrastRatio(entry.chrome.textPrimary, entry.chrome.surface), 4.0)
            << entry.id << " chrome";
        EXPECT_GE(theme::ContrastRatio(entry.chrome.textSecondary, entry.chrome.surface),
                  3.0)
            << entry.id << " secondary";
    }
}

TEST(ThemeCatalog, ColorHelpers)
{
    EXPECT_EQ(theme::ColorToCss(theme::Rgb(1, 2, 3)), "#010203");
    EXPECT_EQ(theme::ColorToCss(theme::Rgba(255, 255, 255, 128)),
              "rgba(255,255,255,0.502)");

    const theme::ThemeColor blended = theme::Blend(theme::Rgb(0, 0, 0),
                                                   theme::Rgb(255, 255, 255), 0.5);
    EXPECT_EQ(blended.r, 128);
    EXPECT_EQ(blended.g, 128);
    EXPECT_EQ(blended.b, 128);

    const theme::ThemeColor over = theme::Over(theme::Rgba(0, 0, 0, 128),
                                               theme::Rgb(255, 255, 255));
    EXPECT_EQ(over.r, 127);
    EXPECT_EQ(over.a, 255);
}
