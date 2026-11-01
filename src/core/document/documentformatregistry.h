#pragma once

#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "document/idocumentloader.h"

namespace awm {

/// Registers document loaders by format and dispatches by file extension.
///
/// This is the extension point for adding a JSONL loader in the future: adding
/// a loader here is enough for the rest of the application to support it.
class DocumentFormatRegistry {
public:
    void registerLoader(std::unique_ptr<IDocumentLoader> loader);

    std::unique_ptr<IDocument> load(const std::wstring& path, std::error_code& ec);

    /// Registry pre-populated with the built-in loaders (Markdown now; JSONL later).
    static DocumentFormatRegistry createDefault();

private:
    std::vector<std::unique_ptr<IDocumentLoader>> loaders_;
};

} // namespace awm
