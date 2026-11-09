#include <gtest/gtest.h>

#include <set>
#include <string>

#include "markdown/previewcss.h"
#include "theme/theme_catalog.h"

using namespace awm;

TEST(ThemeCatalog, HasAllThemesAndGithubDefault)
{
    const auto& themes = theme::AllThemes();
    ASSERT_EQ(themes.size(), 53u);

    std::set<std::string> ids;
    std::set<std::string> names;
    for (const theme::ThemeDefinition& entry : themes) {
        EXPECT_FALSE(std::string(entry.id).empty());
        EXPECT_TRUE(ids.insert(entry.id).second) << "duplicate id " << entry.id;
        EXPECT_TRUE(names.insert(entry.displayName).second)
            << "duplicate display name " << entry.displayName;
        EXPECT_FALSE(entry.author.empty()) << entry.id;
    }

    EXPECT_EQ(std::string(theme::DefaultTheme().id), "github-light");
    EXPECT_EQ(std::string(theme::kDefaultThemeId), "github-light");
}

TEST(ThemeCatalog, DarkFlagAndTokenColorsAreConsistent)
{
    for (const theme::ThemeDefinition& entry : theme::AllThemes()) {
        const bool backgroundIsDark =
            theme::RelativeLuminance(entry.preview.pageBg) < 0.5;
        EXPECT_EQ(entry.dark, backgroundIsDark) << entry.id;

        const theme::TokenColors& tokens = entry.tokens;
        const theme::ThemeColor* all[] = {
            &tokens.comment,       &tokens.punctuation, &tokens.tag,
            &tokens.property,      &tokens.booleanNumber, &tokens.string,
            &tokens.oper,          &tokens.function,    &tokens.keyword,
            &tokens.regex,
        };
        for (const theme::ThemeColor* color : all) {
            EXPECT_EQ(color->a, 255) << entry.id;
        }
    }
}

TEST(ThemeCatalog, GroupingCoversEveryThemeExactlyOnce)
{
    const auto& themes = theme::AllThemes();
    std::set<int> seen;

    const std::vector<int> light = theme::LightThemeIndices();
    const std::vector<int> dark = theme::DarkThemeIndices();
    EXPECT_EQ(light.size() + dark.size(), themes.size());
    for (int index : light) {
        EXPECT_TRUE(seen.insert(index).second);
    }
    for (int index : dark) {
        EXPECT_TRUE(seen.insert(index).second);
    }
    EXPECT_EQ(seen.size(), themes.size());

    seen.clear();
    const std::vector<theme::ThemeGroup> authorGroups = theme::ThemesGroupedByAuthor();
    ASSERT_GE(authorGroups.size(), 2u);
    EXPECT_EQ(authorGroups[0].label, "GitHub");
    EXPECT_EQ(authorGroups[1].label, "Microsoft");
    for (const theme::ThemeGroup& group : authorGroups) {
        EXPECT_FALSE(group.label.empty());
        EXPECT_FALSE(group.indices.empty());
        for (int index : group.indices) {
            EXPECT_TRUE(seen.insert(index).second) << group.label;
        }
    }
    EXPECT_EQ(seen.size(), themes.size());

    seen.clear();
    const std::vector<theme::FamilyThemeGroup> familyGroups =
        theme::ThemesGroupedByFamily();
    ASSERT_FALSE(familyGroups.empty());
    for (std::size_t i = 1; i < familyGroups.size(); ++i) {
        EXPECT_LT(static_cast<int>(familyGroups[i - 1].family),
                  static_cast<int>(familyGroups[i].family));
    }
    for (const theme::FamilyThemeGroup& group : familyGroups) {
        EXPECT_FALSE(group.indices.empty());
        for (int index : group.indices) {
            EXPECT_TRUE(seen.insert(index).second);
        }
    }
    EXPECT_EQ(seen.size(), themes.size());
}

TEST(ThemeCatalog, RepresentativeThemesExist)
{
    for (const char* id : {"ayu-dark", "catppuccin-mocha", "tokyo-night-light",
                           "vscode-high-contrast-light", "github-dark"}) {
        EXPECT_GE(theme::IndexOfTheme(id), 6) << id;
    }
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
        EXPECT_NE(css.find(".awm-copy"), std::string::npos) << entry.id;
        EXPECT_NE(css.find(".awm-copy.awm-copied"), std::string::npos) << entry.id;
        EXPECT_NE(css.find("pre:hover .awm-copy"), std::string::npos) << entry.id;
        EXPECT_NE(css.find("scrollbar-face-color:"), std::string::npos) << entry.id;
        EXPECT_NE(css.find("scrollbar-track-color:"), std::string::npos) << entry.id;
        EXPECT_EQ(css.find("var("), std::string::npos) << entry.id;
        EXPECT_EQ(css.find(":root"), std::string::npos) << entry.id;
        EXPECT_EQ(css.find("--"), std::string::npos) << entry.id;
    }
}

TEST(ThemeCatalog, CopyIconStaysLegibleOnEveryCodeBlock)
{
    // The copy icon is translucent over the code-block background; the theme
    // picks the lowest opacity that still clears the WCAG non-text floor.
    for (const theme::ThemeDefinition& entry : theme::AllThemes()) {
        const double opacity = CopyIconOpacity(entry);
        EXPECT_GE(opacity, 0.55) << entry.id;
        // solarized-light's code text is low-contrast, so it needs 0.9; the
        // glyph is still translucent and no theme reaches full opacity.
        EXPECT_LE(opacity, 0.9) << entry.id << " opacity=" << opacity;
        const theme::ThemeColor icon =
            theme::Blend(entry.preview.codeBlockBg, entry.preview.codeBlockText, opacity);
        EXPECT_GE(theme::ContrastRatio(icon, entry.preview.codeBlockBg), 3.0)
            << entry.id;
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
