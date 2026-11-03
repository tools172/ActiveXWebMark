#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "util/fileutil.h"

using namespace awm;

namespace {

// Dedicated temp directory, removed before and after each test.
class TempDirectory {
public:
    TempDirectory()
        : path_(std::filesystem::temp_directory_path() / "awm_fileutil_test")
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST(FileUtil, WriteReadRoundTripPreservesUtf8)
{
    TempDirectory directory;
    const std::filesystem::path file = directory.path() / "中文.md";
    const std::string content = "# 标题\n\n正文 with UTF-8.\n";

    std::error_code ec;
    ASSERT_TRUE(writeFile(file.wstring(), content, ec)) << ec.message();

    const auto read = readFile(file.wstring(), ec);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, content);
}

TEST(FileUtil, WriteOverwritesExistingFile)
{
    TempDirectory directory;
    const std::filesystem::path file = directory.path() / "doc.md";

    std::error_code ec;
    ASSERT_TRUE(writeFile(file.wstring(), "first", ec));
    ASSERT_TRUE(writeFile(file.wstring(), "second", ec));

    const auto read = readFile(file.wstring(), ec);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, "second");
}

TEST(FileUtil, ReadStripsUtf8Bom)
{
    TempDirectory directory;
    const std::filesystem::path file = directory.path() / "bom.md";
    {
        std::ofstream out(file, std::ios::binary);
        const char bom[] = "\xEF\xBB\xBF# Hello\n";
        out.write(bom, sizeof(bom) - 1);
    }

    std::error_code ec;
    const auto read = readFile(file.wstring(), ec);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, "# Hello\n");
}

TEST(FileUtil, CreatesMissingParentDirectories)
{
    TempDirectory directory;
    const std::filesystem::path file =
        directory.path() / "nested" / "deeper" / "doc.md";

    std::error_code ec;
    ASSERT_TRUE(writeFile(file.wstring(), "x", ec)) << ec.message();
    EXPECT_TRUE(std::filesystem::exists(file));
}

TEST(FileUtil, ReportsFailureWhenParentIsAFile)
{
    TempDirectory directory;
    const std::filesystem::path blocker = directory.path() / "blocker";
    {
        std::ofstream out(blocker);
        out << "not a directory";
    }

    std::error_code ec;
    EXPECT_FALSE(writeFile((blocker / "child.md").wstring(), "x", ec));
    EXPECT_TRUE(static_cast<bool>(ec));
}
