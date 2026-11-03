#include "session/document_session.h"

#include <utility>

#include "util/fileutil.h"
#include "util/pathutil.h"
#include "util/stringutil.h"

namespace awm {

DocumentSession::DocumentSession(std::wstring path, std::wstring fallbackTitle,
                                 std::string source, Outline outline)
    : path_(std::move(path))
    , fallbackTitle_(std::move(fallbackTitle))
    , title_(fallbackTitle_)
    , source_(std::move(source))
    , savedSource_(source_)
    , outline_(std::move(outline))
    , dirty_(false)
{
}

std::unique_ptr<DocumentSession> DocumentSession::fromDocument(std::wstring path,
                                                               const IDocument& document)
{
    std::wstring title = utf8ToWide(document.title());
    if (title.empty()) {
        title = fileStem(path);
    }
    if (title.empty()) {
        title = L"未命名";
    }
    return std::unique_ptr<DocumentSession>(new DocumentSession(
        std::move(path), std::move(title), document.markdownSource(), document.outline()));
}

std::unique_ptr<DocumentSession> DocumentSession::untitled(std::wstring title)
{
    if (title.empty()) {
        title = L"未命名";
    }
    return std::unique_ptr<DocumentSession>(
        new DocumentSession({}, std::move(title), std::string(), Outline{}));
}

void DocumentSession::setSource(std::string source)
{
    source_ = std::move(source);
    dirty_ = source_ != savedSource_;
}

void DocumentSession::markClean()
{
    savedSource_ = source_;
    dirty_ = false;
}

// Picks the first H1 from the current outline, else the file stem. Used after
// Save As so the tab title follows the new file name.
void DocumentSession::updateTitleFromPath()
{
    std::wstring title;
    for (const OutlineItem& item : outline_) {
        if (item.level == 1) {
            title = utf8ToWide(item.text);
            break;
        }
    }
    if (title.empty()) {
        title = fileStem(path_);
    }
    if (title.empty()) {
        title = fallbackTitle_;
    }
    title_ = std::move(title);
}

bool DocumentSession::save(std::error_code& ec)
{
    if (!hasPath()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    if (!writeFile(path_, source_, ec)) {
        return false;
    }
    markClean();
    return true;
}

bool DocumentSession::saveAs(const std::wstring& path, std::error_code& ec)
{
    if (path.empty()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    if (!writeFile(path, source_, ec)) {
        return false;
    }

    path_ = normalizePath(path);
    markClean();
    updateTitleFromPath();
    return true;
}

} // namespace awm
