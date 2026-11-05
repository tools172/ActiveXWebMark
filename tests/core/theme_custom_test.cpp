#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "miniz.h"
#include "theme/theme_catalog.h"
#include "theme/theme_import.h"
#include "theme/theme_json.h"
#include "theme/theme_source.h"
#include "theme/theme_store.h"

using namespace awm::theme;

namespace {

class TempDir {
public:
    explicit TempDir(const wchar_t* name)
        : path_(std::filesystem::temp_directory_path() / name)
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.good()) << path.string();
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void ExpectColor(const ThemeColor& color, int r, int g, int b)
{
    EXPECT_EQ(color.r, r);
    EXPECT_EQ(color.g, g);
    EXPECT_EQ(color.b, b);
}

} // namespace

TEST(ThemeColorParsing, AcceptsCssForms)
{
    ExpectColor(*ParseColor("#123456"), 0x12, 0x34, 0x56);
    ExpectColor(*ParseColor("#abc"), 0xaa, 0xbb, 0xcc);
    ExpectColor(*ParseColor("rgb(1, 2, 3)"), 1, 2, 3);
    const ThemeColor rgba = *ParseColor("rgba(10,20,30,0.5)");
    ExpectColor(rgba, 10, 20, 30);
    EXPECT_EQ(rgba.a, 128);
    EXPECT_FALSE(ParseColor("not-a-color").has_value());
}

TEST(ThemeSource, FinalizeDerivesMissingColors)
{
    ThemeSourceDraft draft;
    draft.name = "Derived";
    draft.background = Rgb(255, 255, 255);
    draft.text = Rgb(17, 17, 17);
    const ThemeSource source = FinalizeSource(std::move(draft));

    EXPECT_EQ(source.name, "Derived");
    EXPECT_FALSE(source.dark);
    EXPECT_EQ(source.link, source.accent);
    EXPECT_LT(source.border.r, source.background.r);
    EXPECT_GT(source.border.r, source.text.r);
    EXPECT_GE(ContrastRatio(source.text, source.background), 4.0);
    EXPECT_FALSE(source.typography.bodyFont.empty());
    EXPECT_FALSE(source.typography.codeFont.empty());
}

TEST(ThemeSource, UserThemeContrastFloors)
{
    ThemeSourceDraft draft;
    draft.name = "Low Contrast";
    draft.background = Rgb(119, 119, 119);
    draft.text = Rgb(136, 136, 136);
    draft.accent = Rgb(136, 136, 136);
    const ThemeDefinition definition =
        BuildDefinition(FinalizeSource(std::move(draft)), true);

    EXPECT_GE(ContrastRatio(definition.preview.pageText, definition.preview.pageBg), 4.5);
    EXPECT_GE(ContrastRatio(definition.chrome.textPrimary, definition.chrome.surface), 4.5);
    EXPECT_GE(ContrastRatio(definition.preview.mutedText, definition.preview.pageBg), 3.0);
    EXPECT_GE(ContrastRatio(definition.chrome.textSecondary, definition.chrome.surface), 3.0);
}

TEST(ThemeSource, RejectsDangerousExtraCss)
{
    std::vector<std::string> warnings;
    EXPECT_FALSE(SanitizeThemeCss("h1{letter-spacing:0;}", warnings).empty());
    EXPECT_TRUE(SanitizeThemeCss("@import url(x);", warnings).empty());
    EXPECT_TRUE(SanitizeThemeCss("h1{background:url(https://x);}", warnings).empty());
    EXPECT_TRUE(SanitizeThemeCss("</style>", warnings).empty());
    EXPECT_FALSE(warnings.empty());
}

TEST(ThemeSource, SlugAndIdAreAsciiAndUnique)
{
    EXPECT_EQ(ThemeSlug("Ocean Blue"), "ocean-blue");
    EXPECT_EQ(ThemeSlug("中文主题"), "theme");
    const std::string first = MakeThemeId("Ocean", "");
    EXPECT_TRUE(first.rfind("user-ocean-", 0) == 0);
    EXPECT_NE(first, MakeThemeId("Ocean", first + "\n"));
}

TEST(ThemeSource, BuiltinsRoundTripExactly)
{
    for (const ThemeDefinition& definition : AllThemes()) {
        const ThemeSource first = SourceFromDefinition(definition, true);
        const ThemeDefinition rebuilt = BuildDefinition(first, false);
        const ThemeSource second = SourceFromDefinition(rebuilt, true);
        EXPECT_EQ(ThemeSourceToJson(first), ThemeSourceToJson(second))
            << definition.id;
    }
}

TEST(ThemeJson, MinimalNativeThemeParses)
{
    const std::string text = R"({
        "schemaVersion": 1,
        "name": "Minimal",
        "markdown": {
            "background": "#ffffff",
            "text": "#111111",
            "accent": "#0066cc"
        },
        "typography": { "bodySize": 17, "lineHeight": 1.6 }
    })";
    const ThemeLoadResult result = LoadThemeSourceJson(text, "minimal.json");
    ASSERT_TRUE(result.source.has_value()) << result.error;
    EXPECT_EQ(result.source->name, "Minimal");
    ExpectColor(result.source->background, 255, 255, 255);
    ExpectColor(result.source->text, 17, 17, 17);
    ExpectColor(result.source->accent, 0, 102, 204);
    EXPECT_EQ(result.source->typography.bodySize, 17);
    EXPECT_DOUBLE_EQ(result.source->typography.lineHeight, 1.6);
}

TEST(ThemeJson, RejectsUnsupportedSchemaAndMissingFields)
{
    ThemeLoadResult version = LoadThemeSourceJson(
        R"({"schemaVersion":2,"name":"X","markdown":{"background":"#fff","text":"#000"}})");
    EXPECT_FALSE(version.source.has_value());

    ThemeLoadResult missing = LoadThemeSourceJson(
        R"({"schemaVersion":1,"markdown":{"background":"#fff","text":"#000"}})");
    EXPECT_FALSE(missing.source.has_value());

    ThemeLoadResult noColors =
        LoadThemeSourceJson(R"({"schemaVersion":1,"name":"X"})");
    EXPECT_FALSE(noColors.source.has_value());
}

TEST(ThemeJson, ReadsFullThemeAndRoundTrips)
{
    const std::string text = R"({
        "schemaVersion": 1,
        "name": "Full",
        "author": "Me",
        "dark": true,
        "family": "Purple",
        "markdown": {
            "background": "#101010",
            "text": "#eeeeee",
            "accent": "#aa66ff",
            "headings": ["#ffffff", "#f0f0f0", "#e0e0e0", "#d0d0d0", "#c0c0c0", "#b0b0b0"],
            "code": { "background": "#181818", "text": "#eeeeee" },
            "blockquote": { "text": "#aaaaaa", "border": "#aa66ff" }
        },
        "tokens": { "comment": "#888888", "keyword": "#ff66aa" },
        "typography": { "bodyFont": "Georgia,serif", "bodySize": 16 },
        "extraCss": "h1{letter-spacing:0;}",
        "ui": { "titleBarBg": "#050505" }
    })";
    const ThemeLoadResult result = LoadThemeSourceJson(text, "full.json");
    ASSERT_TRUE(result.source.has_value()) << result.error;
    EXPECT_TRUE(result.source->dark);
    EXPECT_EQ(result.source->family, ThemeFamily::Purple);
    ExpectColor(result.source->tokens.comment, 0x88, 0x88, 0x88);
    ExpectColor(result.source->tokens.keyword, 0xff, 0x66, 0xaa);
    EXPECT_EQ(result.source->typography.bodyFont, "Georgia,serif");
    ASSERT_TRUE(result.source->ui.titleBarBg.has_value());
    ExpectColor(*result.source->ui.titleBarBg, 5, 5, 5);

    const ThemeLoadResult again =
        LoadThemeSourceJson(ThemeSourceToJson(*result.source), "again.json");
    ASSERT_TRUE(again.source.has_value()) << again.error;
    EXPECT_EQ(ThemeSourceToJson(*result.source), ThemeSourceToJson(*again.source));
}

TEST(ThemeJson, JsoncCommentsAndTrailingCommas)
{
    const std::string input = R"({
        // comment
        "a": "http://example.com//not-a-comment",
        /* block */
        "b": [1, 2,],
    })";
    const std::string cleaned = StripJsonComments(input);
    EXPECT_NE(cleaned.find("\"http://example.com//not-a-comment\""), std::string::npos);
    EXPECT_EQ(cleaned.find("// comment"), std::string::npos);
    EXPECT_EQ(cleaned.find("/* block */"), std::string::npos);
    EXPECT_EQ(cleaned.find("2,]"), std::string::npos);
}

TEST(ThemeStore, SaveLoadDeleteAndInvalidFiles)
{
    TempDir dir(L"awm_theme_store_test");

    ThemeSourceDraft draft;
    draft.name = "Store Test";
    draft.background = Rgb(32, 32, 32);
    draft.text = Rgb(230, 230, 230);
    draft.accent = Rgb(0, 128, 255);
    draft.dark = true;
    ThemeSource source = FinalizeSource(std::move(draft));

    std::string error;
    ASSERT_TRUE(SaveThemeToStore(dir.path(), source, error)) << error;
    ASSERT_FALSE(source.id.empty());
    EXPECT_TRUE(source.id.rfind("user-", 0) == 0);

    ThemeStoreLoadResult loaded = LoadThemeStore(dir.path());
    ASSERT_EQ(loaded.themes.size(), 1u) << (loaded.errors.empty() ? "" : loaded.errors[0]);
    EXPECT_EQ(loaded.themes[0].source.id, source.id);
    EXPECT_EQ(loaded.themes[0].definition.displayName, "Store Test");
    EXPECT_TRUE(loaded.themes[0].definition.userDefined);

    WriteText(dir.path() / "bad.json", "not json");
    loaded = LoadThemeStore(dir.path());
    EXPECT_EQ(loaded.themes.size(), 1u);
    EXPECT_FALSE(loaded.errors.empty());

    ASSERT_TRUE(DeleteThemeFromStore(dir.path(), source.id, error)) << error;
    loaded = LoadThemeStore(dir.path());
    EXPECT_TRUE(loaded.themes.empty());
}

TEST(ThemeStore, LoadsHandWrittenNativeFile)
{
    TempDir dir(L"awm_theme_store_native_test");
    WriteText(dir.path() / L"user-smoke-dark.awmtheme.json", R"({
      "schemaVersion": 1,
      "id": "user-smoke-dark",
      "name": "Smoke Dark",
      "author": "smoke",
      "dark": true,
      "markdown": {
        "background": "#101010",
        "text": "#eeeeee",
        "accent": "#3399ff",
        "link": "#66bbff"
      }
    })");

    const ThemeStoreLoadResult loaded = LoadThemeStore(dir.path());
    ASSERT_EQ(loaded.themes.size(), 1u)
        << (loaded.errors.empty() ? "no errors" : loaded.errors[0]);
    EXPECT_EQ(loaded.themes[0].source.id, "user-smoke-dark");
    EXPECT_EQ(loaded.themes[0].definition.displayName, "Smoke Dark");
    EXPECT_TRUE(loaded.themes[0].definition.dark);
    ExpectColor(loaded.themes[0].definition.preview.pageBg, 0x10, 0x10, 0x10);
}

TEST(ThemeStore, AssignsNewIdWhenCollidingWithBuiltin)
{
    TempDir dir(L"awm_theme_store_collision_test");
    ThemeSourceDraft draft;
    draft.id = DefaultTheme().id;
    draft.name = "Collision";
    draft.background = Rgb(255, 255, 255);
    draft.text = Rgb(0, 0, 0);
    ThemeSource source = FinalizeSource(std::move(draft));

    std::string error;
    ASSERT_TRUE(SaveThemeToStore(dir.path(), source, error)) << error;
    EXPECT_NE(source.id, DefaultTheme().id);
    EXPECT_TRUE(source.id.rfind("user-", 0) == 0);
}

TEST(ThemeCatalog, UserThemesAppendToTheTail)
{
    ClearUserThemes();
    const std::size_t builtins = BuiltinThemeCount();

    ThemeSourceDraft draft;
    draft.id = "user-test-theme";
    draft.name = "User Test";
    draft.background = Rgb(20, 20, 20);
    draft.text = Rgb(240, 240, 240);
    const ThemeDefinition definition =
        BuildDefinition(FinalizeSource(std::move(draft)), true);

    const unsigned before = CatalogGeneration();
    SetUserThemes({definition});
    EXPECT_EQ(AllThemes().size(), builtins + 1);
    EXPECT_GT(CatalogGeneration(), before);
    EXPECT_NE(FindTheme("user-test-theme"), nullptr);
    EXPECT_TRUE(ResolveTheme("user-test-theme").userDefined);

    ClearUserThemes();
    EXPECT_EQ(AllThemes().size(), builtins);
    EXPECT_EQ(FindTheme("user-test-theme"), nullptr);
}

TEST(ThemeImport, VscodeJsoncIncludeAndTokens)
{
    TempDir dir(L"awm_theme_import_vscode_test");
    WriteText(dir.path() / L"base.json", R"({
        "name": "Base",
        "type": "dark",
        "colors": {
            "editor.background": "#101010",
            "editor.foreground": "#eeeeee"
        },
        "tokenColors": [
            { "scope": "comment", "settings": { "foreground": "#888888" } }
        ]
    })");
    WriteText(dir.path() / L"child.jsonc", R"({
        // inherited theme
        "name": "Child",
        "include": "./base.json",
        "colors": {
            "editor.background": "#202020",
        },
    })");

    const ImportResult result = ImportThemeFile(dir.path() / L"child.jsonc");
    ASSERT_TRUE(result.error.empty()) << result.error;
    ASSERT_EQ(result.themes.size(), 1u);
    const ThemeSource& theme = result.themes[0].source;
    EXPECT_EQ(theme.name, "Child");
    EXPECT_TRUE(theme.dark);
    ExpectColor(theme.background, 0x20, 0x20, 0x20);
    ExpectColor(theme.text, 0xee, 0xee, 0xee);
    ExpectColor(theme.tokens.comment, 0x88, 0x88, 0x88);
}

TEST(ThemeImport, Base16Yaml)
{
    TempDir dir(L"awm_theme_import_base16_test");
    WriteText(dir.path() / L"scheme.yaml", R"(
system: "base16"
name: "Test Base"
author: "Me"
variant: "dark"
palette:
  base00: "101010"
  base01: "181818"
  base03: "888888"
  base05: "eeeeee"
  base08: "ff5555"
  base0B: "55ff55"
  base0D: "3399ff"
  base0E: "aa66ff"
)");

    const ImportResult result = ImportThemeFile(dir.path() / L"scheme.yaml");
    ASSERT_TRUE(result.error.empty()) << result.error;
    ASSERT_EQ(result.themes.size(), 1u);
    const ThemeSource& theme = result.themes[0].source;
    EXPECT_EQ(theme.name, "Test Base");
    EXPECT_EQ(theme.author, "Me");
    EXPECT_TRUE(theme.dark);
    ExpectColor(theme.background, 0x10, 0x10, 0x10);
    ExpectColor(theme.text, 0xee, 0xee, 0xee);
    ExpectColor(theme.accent, 0x33, 0x99, 0xff);
}

TEST(ThemeImport, TextMateTmTheme)
{
    TempDir dir(L"awm_theme_import_tmtheme_test");
    WriteText(dir.path() / L"theme.tmTheme", R"(<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
  <key>name</key><string>TM Test</string>
  <key>settings</key>
  <array>
    <dict>
      <key>settings</key>
      <dict>
        <key>background</key><string>#272822</string>
        <key>foreground</key><string>#F8F8F2</string>
      </dict>
    </dict>
    <dict>
      <key>scope</key><string>comment</string>
      <key>settings</key><dict><key>foreground</key><string>#75715E</string></dict>
    </dict>
  </array>
</dict>
</plist>)");

    const ImportResult result = ImportThemeFile(dir.path() / L"theme.tmTheme");
    ASSERT_TRUE(result.error.empty()) << result.error;
    ASSERT_EQ(result.themes.size(), 1u);
    const ThemeSource& theme = result.themes[0].source;
    EXPECT_EQ(theme.name, "TM Test");
    ExpectColor(theme.background, 0x27, 0x28, 0x22);
    ExpectColor(theme.text, 0xF8, 0xF8, 0xF2);
    ExpectColor(theme.tokens.comment, 0x75, 0x71, 0x5E);
}

TEST(ThemeImport, VsixPackage)
{
    const std::string package = R"({
        "name": "test-theme",
        "publisher": "acme",
        "version": "1.0.0",
        "license": "MIT",
        "contributes": {
            "themes": [{
                "label": "Acme Dark",
                "uiTheme": "vs-dark",
                "path": "./themes/acme.json"
            }]
        }
    })";
    const std::string theme = R"({
        "name": "Acme Dark",
        "type": "dark",
        "colors": {
            "editor.background": "#123456",
            "editor.foreground": "#ffffff"
        }
    })";

    mz_zip_archive zip = {};
    ASSERT_TRUE(mz_zip_writer_init_heap(&zip, 0, 0));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "extension/package.json", package.data(),
                                      package.size(), MZ_BEST_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "extension/themes/acme.json", theme.data(),
                                      theme.size(), MZ_BEST_COMPRESSION));
    void* buffer = nullptr;
    size_t size = 0;
    ASSERT_TRUE(mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size));
    std::vector<std::uint8_t> bytes(static_cast<std::uint8_t*>(buffer),
                                    static_cast<std::uint8_t*>(buffer) + size);
    mz_free(buffer);
    mz_zip_writer_end(&zip);

    TempDir dir(L"awm_theme_import_vsix_test");
    const std::filesystem::path path = dir.path() / L"acme.vsix";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.good());
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    const ImportResult result = ImportThemeFile(path);
    ASSERT_TRUE(result.error.empty()) << result.error;
    ASSERT_EQ(result.themes.size(), 1u);
    const ThemeSource& imported = result.themes[0].source;
    EXPECT_EQ(imported.name, "Acme Dark");
    EXPECT_EQ(imported.author, "acme");
    EXPECT_EQ(imported.license, "MIT");
    EXPECT_NE(imported.source.find("VS Code Marketplace"), std::string::npos);
    ExpectColor(imported.background, 0x12, 0x34, 0x56);
}
