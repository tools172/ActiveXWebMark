#pragma once

#include <memory>
#include <string>
#include <system_error>

#include "document/idocumentloader.h"

namespace awm {

/// Loads Markdown documents (.md, .markdown, .mdown).
class MarkdownLoader : public IDocumentLoader {
public:
    bool canLoad(const std::wstring& path) const override;
    std::unique_ptr<IDocument> load(const std::wstring& path, std::error_code& ec) override;
};

} // namespace awm
