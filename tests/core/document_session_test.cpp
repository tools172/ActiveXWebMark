#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "document/headingparser.h"
#include "document/markdowndocument.h"
#include "session/document_session.h"
#include "util/fileutil.h"

using namespace awm;

namespace {

class TempDirectory {
public:
    TempDirectory()
        : path_(std::filesystem::temp_directory_path() / "awm_session_test")
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

std::unique_ptr<DocumentSession> sessionFromMarkdown(const std::wstring& path,
                                                     const std::string& markdown)
{
    Outline outline = extractHeadings(markdown);
    makeAnchorsUnique(outline);
    MarkdownDocument document("Title", markdown, outline);
    return DocumentSession::fromDocument(path, document);
}

} // namespace

TEST(DocumentSession, FromDocumentIsCleanAndCarriesContent)
{
    auto session = sessionFromMarkdown(L"C:\\docs\\a.md", "# Title\n\nBody\n");
    ASSERT_TRUE(session != nullptr);
    EXPECT_FALSE(session->isDirty());
    EXPECT_EQ(session->path(), L"C:\\docs\\a.md");
    EXPECT_EQ(session->title(), L"Title");
    EXPECT_EQ(session->source(), "# Title\n\nBody\n");
    ASSERT_EQ(session->outline().size(), 1u);
    EXPECT_EQ(session->outline()[0].text, "Title");
}

TEST(DocumentSession, EditMarksDirtyAndRevertClearsIt)
{
    const std::string original = "# Title\n";
    auto session = sessionFromMarkdown(L"C:\\docs\\a.md", original);
    session->setSource("# Title\nmore\n");
    EXPECT_TRUE(session->isDirty());

    session->setSource(original);
    EXPECT_FALSE(session->isDirty());
}

TEST(DocumentSession, MarkDirtySetsFlagWithoutReplacingSource)
{
    auto session = sessionFromMarkdown(L"C:\\docs\\a.md", "# Title\n");
    session->markDirty();
    EXPECT_TRUE(session->isDirty());
    EXPECT_EQ(session->source(), "# Title\n");
}

TEST(DocumentSession, SaveWritesSourceAndClearsDirty)
{
    TempDirectory directory;
    const std::filesystem::path file = directory.path() / "doc.md";
    auto session = sessionFromMarkdown(file.wstring(), "# Title\n");
    session->setSource("# Changed\n");

    std::error_code ec;
    ASSERT_TRUE(session->save(ec)) << ec.message();
    EXPECT_FALSE(session->isDirty());

    const auto read = readFile(file.wstring(), ec);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, "# Changed\n");
}

TEST(DocumentSession, SaveWithoutPathFails)
{
    auto session = DocumentSession::untitled(L"未命名");
    std::error_code ec;
    EXPECT_FALSE(session->save(ec));
    EXPECT_EQ(ec, std::make_error_code(std::errc::invalid_argument));
}

TEST(DocumentSession, SaveAsRebindsPathAndUsesStemWhenNoHeading)
{
    TempDirectory directory;
    auto session = DocumentSession::untitled(L"未命名");
    session->setSource("plain text\n");

    const std::filesystem::path file = directory.path() / "saved.md";
    std::error_code ec;
    ASSERT_TRUE(session->saveAs(file.wstring(), ec)) << ec.message();

    EXPECT_TRUE(session->hasPath());
    EXPECT_FALSE(session->isDirty());
    EXPECT_EQ(session->title(), L"saved");
    EXPECT_EQ(session->path(), std::filesystem::weakly_canonical(file).wstring());
}

TEST(DocumentSession, SaveAsKeepsFirstHeadingAsTitle)
{
    TempDirectory directory;
    auto session = sessionFromMarkdown(L"C:\\docs\\a.md", "# Heading Title\n");
    const std::filesystem::path file = directory.path() / "renamed.md";

    std::error_code ec;
    ASSERT_TRUE(session->saveAs(file.wstring(), ec)) << ec.message();
    EXPECT_EQ(session->title(), L"Heading Title");
}

TEST(DocumentSession, FailedSaveAsKeepsDirtyStateAndOldPath)
{
    TempDirectory directory;
    const std::filesystem::path blocker = directory.path() / "blocker";
    {
        std::ofstream out(blocker);
        out << "file";
    }

    auto session = sessionFromMarkdown(L"C:\\docs\\a.md", "# A\n");
    session->setSource("# B\n");

    std::error_code ec;
    EXPECT_FALSE(session->saveAs((blocker / "child.md").wstring(), ec));
    EXPECT_TRUE(static_cast<bool>(ec));
    EXPECT_TRUE(session->isDirty());
    EXPECT_EQ(session->path(), L"C:\\docs\\a.md");
}
