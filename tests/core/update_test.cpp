#include <gtest/gtest.h>

#include <string>

#include "update/updateinfo.h"

using namespace awm;

namespace {

SemanticVersion parse(const std::string& text)
{
    SemanticVersion version;
    EXPECT_TRUE(parseSemanticVersion(text, version)) << text;
    return version;
}

int compare(const std::string& a, const std::string& b)
{
    return compareSemanticVersions(parse(a), parse(b));
}

const char* kReleaseJson = R"json(
{
  "tag_name": "v0.3.0",
  "name": "ActiveXWebMark 0.3.0",
  "body": "## 新功能\n- 自动检查更新\n- 下载安装包",
  "html_url": "https://github.com/tools172/ActiveXWebMark/releases/tag/v0.3.0",
  "published_at": "2025-01-02T03:04:05Z",
  "assets": [
    {
      "name": "ActiveXWebMark-0.3.0.msi",
      "browser_download_url": "https://github.com/tools172/ActiveXWebMark/releases/download/v0.3.0/ActiveXWebMark-0.3.0.msi",
      "size": 123456,
      "digest": "sha256:aaaa"
    },
    {
      "name": "ActiveXWebMark-0.3.0-setup.exe",
      "browser_download_url": "https://github.com/tools172/ActiveXWebMark/releases/download/v0.3.0/ActiveXWebMark-0.3.0-setup.exe",
      "size": 234567
    }
  ]
}
)json";

} // namespace

TEST(VersionTag, NormalizesLeadingV)
{
    EXPECT_EQ(normalizeVersionTag("v1.2.3"), "1.2.3");
    EXPECT_EQ(normalizeVersionTag("V1.2.3"), "1.2.3");
    EXPECT_EQ(normalizeVersionTag("  1.2.3 \n"), "1.2.3");
    EXPECT_EQ(normalizeVersionTag("version-1"), "version-1");
    EXPECT_EQ(normalizeVersionTag("v"), "v");
    EXPECT_EQ(normalizeVersionTag(""), "");
}

TEST(SemanticVersion, ParsesCommonForms)
{
    const SemanticVersion majorOnly = parse("1");
    EXPECT_EQ(majorOnly.major, 1ULL);
    EXPECT_EQ(majorOnly.minor, 0ULL);
    EXPECT_EQ(majorOnly.patch, 0ULL);

    const SemanticVersion full = parse("v1.2.3");
    EXPECT_EQ(full.major, 1ULL);
    EXPECT_EQ(full.minor, 2ULL);
    EXPECT_EQ(full.patch, 3ULL);
    EXPECT_TRUE(full.prerelease.empty());

    const SemanticVersion partial = parse("1.2");
    EXPECT_EQ(partial.major, 1ULL);
    EXPECT_EQ(partial.minor, 2ULL);
    EXPECT_EQ(partial.patch, 0ULL);

    const SemanticVersion pre = parse("1.2.3-rc.1+build.9");
    EXPECT_EQ(pre.prerelease, "rc.1");
}

TEST(SemanticVersion, RejectsInvalidText)
{
    SemanticVersion version;
    EXPECT_FALSE(parseSemanticVersion("", version));
    EXPECT_FALSE(parseSemanticVersion("v", version));
    EXPECT_FALSE(parseSemanticVersion("1.2.3.4", version));
    EXPECT_FALSE(parseSemanticVersion("1..3", version));
    EXPECT_FALSE(parseSemanticVersion("1.2.x", version));
    EXPECT_FALSE(parseSemanticVersion("1.2.3-", version));
    EXPECT_FALSE(parseSemanticVersion("1.2.3-beta..1", version));
    EXPECT_FALSE(parseSemanticVersion("1.2.3-beta_1", version));
}

TEST(SemanticVersion, ComparesNumericComponents)
{
    EXPECT_EQ(compare("0.2.0", "0.2.0"), 0);
    EXPECT_LT(compare("0.2.0", "0.10.0"), 0);
    EXPECT_GT(compare("0.10.0", "0.9.9"), 0);
    EXPECT_GT(compare("1", "0.9.9"), 0);
    EXPECT_EQ(compare("v1.2.3", "1.2.3"), 0);
}

TEST(SemanticVersion, ComparesPrereleases)
{
    EXPECT_GT(compare("1.0.0", "1.0.0-rc.1"), 0);
    EXPECT_LT(compare("1.0.0-rc.1", "1.0.0"), 0);
    EXPECT_LT(compare("1.0.0-alpha", "1.0.0-beta"), 0);
    EXPECT_LT(compare("1.0.0-rc.2", "1.0.0-rc.10"), 0);
    EXPECT_EQ(compare("1.0.0-rc.1", "1.0.0-rc.1"), 0);
    EXPECT_LT(compare("1.0.0-alpha", "1.0.0-alpha.1"), 0);
    // Numeric identifiers sort before alphanumeric identifiers.
    EXPECT_LT(compare("1.0.0-1", "1.0.0-alpha"), 0);
}

TEST(SemanticVersion, IsVersionNewer)
{
    EXPECT_TRUE(isVersionNewer("v0.3.0", "0.2.0"));
    EXPECT_FALSE(isVersionNewer("0.2.0", "0.2.0"));
    EXPECT_FALSE(isVersionNewer("0.1.9", "0.2.0"));
    EXPECT_FALSE(isVersionNewer("not-a-version", "0.2.0"));
    EXPECT_FALSE(isVersionNewer("0.3.0", "not-a-version"));
}

TEST(ReleaseJson, ParsesLatestReleasePayload)
{
    ReleaseInfo release;
    std::string error;
    ASSERT_TRUE(parseReleaseJson(kReleaseJson, release, error)) << error;

    EXPECT_EQ(release.tag, "v0.3.0");
    EXPECT_EQ(release.version, "0.3.0");
    EXPECT_EQ(release.name, "ActiveXWebMark 0.3.0");
    EXPECT_EQ(release.releaseUrl,
              "https://github.com/tools172/ActiveXWebMark/releases/tag/v0.3.0");
    EXPECT_EQ(release.publishedAt, "2025-01-02T03:04:05Z");
    EXPECT_NE(release.notes.find("自动检查更新"), std::string::npos);
    ASSERT_EQ(release.assets.size(), 2U);

    EXPECT_EQ(release.assets[0].size, 123456);
    EXPECT_EQ(release.assets[0].digest, "sha256:aaaa");
    EXPECT_EQ(release.assets[1].size, 234567);
    EXPECT_TRUE(release.assets[1].digest.empty());
}

TEST(ReleaseJson, ChoosesSetupExeThenMsi)
{
    ReleaseInfo release;
    std::string error;
    ASSERT_TRUE(parseReleaseJson(kReleaseJson, release, error)) << error;

    const ReleaseAsset* asset = findInstallerAsset(release);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->name, "ActiveXWebMark-0.3.0-setup.exe");

    release.assets.erase(release.assets.begin() + 1); // Remove the setup.
    asset = findInstallerAsset(release);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->name, "ActiveXWebMark-0.3.0.msi");

    release.assets.clear();
    EXPECT_EQ(findInstallerAsset(release), nullptr);
}

TEST(ReleaseJson, PrefersAnyExeWhenNoSetupOrMsi)
{
    ReleaseInfo release;
    std::string error;
    ASSERT_TRUE(parseReleaseJson(R"json({
        "tag_name": "v1.0.0",
        "assets": [
          {"name": "release-notes.txt", "browser_download_url": "https://example.test/notes.txt"},
          {"name": "ActiveXWebMark-1.0.0-portable.exe",
           "browser_download_url": "https://example.test/portable.exe"}
        ]
    })json",
                                 release, error))
        << error;

    const ReleaseAsset* asset = findInstallerAsset(release);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->name, "ActiveXWebMark-1.0.0-portable.exe");
}

TEST(ReleaseJson, RejectsMalformedPayloads)
{
    ReleaseInfo release;
    std::string error;

    EXPECT_FALSE(parseReleaseJson("not json", release, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(parseReleaseJson("[]", release, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(parseReleaseJson(R"json({"name": "missing tag"})json", release, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(parseReleaseJson(R"json({"tag_name": "release-latest"})json", release, error));
    EXPECT_FALSE(error.empty());
}

TEST(ReleaseJson, HandlesMissingOptionalFields)
{
    ReleaseInfo release;
    std::string error;
    ASSERT_TRUE(parseReleaseJson(R"json({"tag_name": "v2.0.0"})json", release, error)) << error;

    EXPECT_EQ(release.version, "2.0.0");
    EXPECT_TRUE(release.name.empty());
    EXPECT_TRUE(release.notes.empty());
    EXPECT_TRUE(release.assets.empty());
}

TEST(ReleaseNotes, SanitizesHyperlinkMarkup)
{
    const std::wstring text =
        releaseNotesForDisplay("  <a href=\"https://evil.test\">click</a>\r\n", 100);
    EXPECT_EQ(text.find(L'<'), std::wstring::npos);
    EXPECT_EQ(text.find(L'>'), std::wstring::npos);
    EXPECT_NE(text.find(L"click"), std::wstring::npos);
    EXPECT_EQ(text.front(), L'＜');
    EXPECT_EQ(text.back(), L'＞');
}

TEST(ReleaseNotes, TruncatesLongNotes)
{
    std::string notes;
    for (int i = 0; i < 50; ++i) {
        notes += "line ";
    }
    const std::wstring text = releaseNotesForDisplay(notes, 20);
    EXPECT_LT(text.size(), notes.size());
    EXPECT_EQ(text.substr(text.size() - 1), L"…");
}
