#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "history/recent_files.h"

#include "fake_settings.h"

using namespace awm;

namespace {

std::wstring tempPath(const wchar_t* name)
{
    return (std::filesystem::temp_directory_path() / L"awm_recent" / name).wstring();
}

} // namespace

TEST(RecentFiles, AddsMostRecentFirst)
{
    RecentFiles recent;
    recent.add(tempPath(L"a.md"));
    recent.add(tempPath(L"b.md"));

    ASSERT_EQ(recent.size(), 2u);
    EXPECT_EQ(std::filesystem::path(recent.entries()[0]).filename(), L"b.md");
    EXPECT_EQ(std::filesystem::path(recent.entries()[1]).filename(), L"a.md");
}

TEST(RecentFiles, DeduplicatesCaseInsensitivelyAndMovesToFront)
{
    const std::wstring a = tempPath(L"Docs\\A.md");
    std::wstring aLower = a;
    for (wchar_t& ch : aLower) {
        if (ch >= L'A' && ch <= L'Z') {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
        if (ch == L'\\') {
            ch = L'/';
        }
    }

    RecentFiles recent;
    recent.add(a);
    recent.add(tempPath(L"b.md"));
    recent.add(aLower);

    ASSERT_EQ(recent.size(), 2u);
    EXPECT_EQ(std::filesystem::path(recent.entries()[0]).filename(), L"A.md");
}

TEST(RecentFiles, RemovesByAnyCase)
{
    RecentFiles recent;
    recent.add(tempPath(L"a.md"));
    EXPECT_TRUE(recent.remove(tempPath(L"A.MD")));
    EXPECT_TRUE(recent.entries().empty());
    EXPECT_FALSE(recent.remove(tempPath(L"a.md")));
}

TEST(RecentFiles, TrimsToCapacity)
{
    RecentFiles recent(20, 3);
    for (int i = 0; i < 5; ++i) {
        recent.add(tempPath((L"doc" + std::to_wstring(i) + L".md").c_str()));
    }
    EXPECT_EQ(recent.size(), 3u);
    EXPECT_EQ(std::filesystem::path(recent.entries()[0]).filename(), L"doc4.md");
    EXPECT_EQ(std::filesystem::path(recent.entries()[2]).filename(), L"doc2.md");
}

TEST(RecentFiles, PagesItems)
{
    RecentFiles recent(2, 10);
    for (int i = 0; i < 5; ++i) {
        recent.add(tempPath((L"d" + std::to_wstring(i) + L".md").c_str()));
    }

    EXPECT_EQ(recent.page(0, 2).size(), 2u);
    EXPECT_EQ(recent.page(4, 2).size(), 1u);
    EXPECT_TRUE(recent.page(5, 2).empty());
    EXPECT_EQ(recent.menuPage().size(), 2u);
}

TEST(RecentFiles, JsonRoundTripPreservesOrderAndUnicode)
{
    RecentFiles recent(20, 10);
    recent.add(L"C:\\文档\\一.md");
    recent.add(L"C:\\docs\\two.md");

    const std::string json = recent.toJson();
    const RecentFiles restored = RecentFiles::fromJson(json, 20, 10);

    ASSERT_EQ(restored.size(), 2u);
    EXPECT_EQ(restored.entries()[0], recent.entries()[0]);
    EXPECT_EQ(restored.entries()[1], recent.entries()[1]);
}

TEST(RecentFiles, MalformedJsonYieldsEmptyList)
{
    EXPECT_TRUE(RecentFiles::fromJson("not json").entries().empty());
    EXPECT_TRUE(RecentFiles::fromJson("{\"a\":1}").entries().empty());
}

TEST(RecentFiles, SavesAndLoadsThroughSettings)
{
    test::FakeSettings settings;

    RecentFiles original;
    original.add(tempPath(L"one.md"));
    original.save(settings, "history.recentFiles");

    RecentFiles restored;
    restored.load(settings, "history.recentFiles");
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_EQ(restored.entries()[0], original.entries()[0]);
}
