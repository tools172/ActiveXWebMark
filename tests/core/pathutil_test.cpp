#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "util/pathutil.h"

using namespace awm;

namespace {

// Builds a NUL-terminated multi-select buffer from wide strings.
class WideBuffer {
public:
    explicit WideBuffer(const std::vector<std::wstring>& parts)
    {
        for (const std::wstring& part : parts) {
            data_.insert(data_.end(), part.begin(), part.end());
            data_.push_back(L'\0');
        }
        data_.push_back(L'\0');
    }

    const wchar_t* data() const { return data_.data(); }
    std::size_t size() const { return data_.size(); }

private:
    std::vector<wchar_t> data_;
};

} // namespace

TEST(PathUtil, SamePathIgnoresCaseAndSeparators)
{
    const std::wstring path =
        (std::filesystem::temp_directory_path() / L"AWM_PathUtil" / L"Doc.md").wstring();
    std::wstring other = path;
    for (wchar_t& ch : other) {
        if (ch >= L'a' && ch <= L'z') {
            ch = static_cast<wchar_t>(ch - L'a' + L'A');
        }
        if (ch == L'\\') {
            ch = L'/';
        }
    }
    EXPECT_TRUE(samePath(path, other));
    EXPECT_EQ(pathKey(path), pathKey(other));
}

TEST(PathUtil, FileNameStemAndDirectory)
{
    const std::wstring path = L"C:\\docs\\notes\\hello.md";
    EXPECT_EQ(fileName(path), L"hello.md");
    EXPECT_EQ(fileStem(path), L"hello");
    EXPECT_EQ(fileDirectory(path), L"C:\\docs\\notes");
}

TEST(PathUtil, FileUrlEncodesSpacesAndUtf8)
{
    EXPECT_EQ(fileUrlFromPath(L"C:\\Docs\\My File.md"),
              "file:///C:/Docs/My%20File.md");
    EXPECT_EQ(fileUrlFromPath(L"C:\\Docs\\a#b?.md"),
              "file:///C:/Docs/a%23b%3F.md");

    const std::string chinese = fileUrlFromPath(L"C:\\文档\\a.md");
    EXPECT_NE(chinese.find("%E6%96%87%E6%A1%A3"), std::string::npos);
}

TEST(PathUtil, FileUrlHandlesUncPaths)
{
    EXPECT_EQ(fileUrlFromPath(L"\\\\server\\share\\a b.md"),
              "file://server/share/a%20b.md");
}

TEST(PathUtil, FileUrlCanBeMarkedAsDirectory)
{
    const std::string url = fileUrlFromPath(L"C:\\Docs", true);
    ASSERT_FALSE(url.empty());
    EXPECT_EQ(url.back(), '/');
}

TEST(PathUtil, ParsesSingleSelection)
{
    const WideBuffer buffer({L"C:\\docs\\a.md"});
    const std::vector<std::wstring> paths = parseMultiSelectPaths(buffer.data(), buffer.size());
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], L"C:\\docs\\a.md");
}

TEST(PathUtil, ParsesMultiSelection)
{
    const WideBuffer buffer({L"C:\\docs", L"a.md", L"b.md"});
    const std::vector<std::wstring> paths = parseMultiSelectPaths(buffer.data(), buffer.size());
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], L"C:\\docs\\a.md");
    EXPECT_EQ(paths[1], L"C:\\docs\\b.md");
}

TEST(PathUtil, EmptyBufferYieldsNoPaths)
{
    EXPECT_TRUE(parseMultiSelectPaths(nullptr, 0).empty());
    const wchar_t buffer[] = {L'\0', L'\0'};
    EXPECT_TRUE(parseMultiSelectPaths(buffer, 2).empty());
}
