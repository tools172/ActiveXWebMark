#include "document/documentformatregistry.h"

#include <utility>

#include "document/markdownloader.h"

namespace awm {

void DocumentFormatRegistry::registerLoader(std::unique_ptr<IDocumentLoader> loader)
{
    loaders_.push_back(std::move(loader));
}

bool DocumentFormatRegistry::canLoad(const std::wstring& path) const
{
    for (const auto& loader : loaders_) {
        if (loader->canLoad(path)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<IDocument> DocumentFormatRegistry::load(const std::wstring& path,
                                                        std::error_code& ec)
{
    for (const auto& loader : loaders_) {
        if (loader->canLoad(path)) {
            return loader->load(path, ec);
        }
    }
    ec = std::make_error_code(std::errc::invalid_argument);
    return nullptr;
}

DocumentFormatRegistry DocumentFormatRegistry::createDefault()
{
    DocumentFormatRegistry registry;
    registry.registerLoader(std::make_unique<MarkdownLoader>());
    return registry;
}

} // namespace awm
