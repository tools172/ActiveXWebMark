#pragma once

#include <string>

#include "document/idocument.h"

namespace awm {

/// A plain Markdown document loaded from disk.
class MarkdownDocument : public IDocument {
public:
    MarkdownDocument(std::string title, std::string markdown, Outline outline);

    std::string title() const override;
    const Outline& outline() const override;
    std::string markdownSource() const override;

private:
    std::string title_;
    std::string markdown_;
    Outline outline_;
};

} // namespace awm
