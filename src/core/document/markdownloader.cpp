#include "document/markdownloader.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

#include "document/headingparser.h"
#include "document/markdowndocument.h"
#include "util/fileutil.h"
#include "util/stringutil.h"

namespace awm {

namespace {

bool endsWith(const std::wstring& value, const wchar_t* suffix)
{
    std::wstring lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    const std::size_t n = std::wcslen(suffix);
    return lower.size() >= n && lower.compare(lower.size() - n, n, suffix) == 0;
}

} // namespace

bool MarkdownLoader::canLoad(const std::wstring& path) const
{
    return endsWith(path, L".md") || endsWith(path, L".markdown") ||
           endsWith(path, L".mdown");
}

std::unique_ptr<IDocument> MarkdownLoader::load(const std::wstring& path, std::error_code& ec)
{
    auto contents = readFile(path, ec);
    if (!contents) {
        return nullptr;
    }

    Outline outline = extractHeadings(*contents);

    // Title: first H1 heading, else the file name (without extension).
    std::string title;
    for (const OutlineItem& item : outline) {
        if (item.level == 1) {
            title = item.text;
            break;
        }
    }
    if (title.empty()) {
        title = wideToUtf8(std::filesystem::path(path).stem().wstring());
    }

    return std::make_unique<MarkdownDocument>(std::move(title), std::move(*contents),
                                              std::move(outline));
}

} // namespace awm
