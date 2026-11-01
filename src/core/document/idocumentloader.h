#pragma once

#include <memory>
#include <string>
#include <system_error>

#include "document/idocument.h"

namespace awm {

/// Loads documents of a particular format (Markdown now; JSONL later).
class IDocumentLoader {
public:
    virtual ~IDocumentLoader() = default;

    /// Whether this loader can load the given file (typically by extension).
    virtual bool canLoad(const std::wstring& path) const = 0;

    /// Loads a document. Returns nullptr and sets `ec` on failure.
    virtual std::unique_ptr<IDocument> load(const std::wstring& path,
                                            std::error_code& ec) = 0;
};

} // namespace awm
