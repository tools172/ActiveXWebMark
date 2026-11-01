#include "document/markdowndocument.h"

#include <utility>

namespace awm {

MarkdownDocument::MarkdownDocument(std::string title, std::string markdown, Outline outline)
    : title_(std::move(title)), markdown_(std::move(markdown)), outline_(std::move(outline))
{
}

std::string MarkdownDocument::title() const
{
    return title_;
}

const Outline& MarkdownDocument::outline() const
{
    return outline_;
}

std::string MarkdownDocument::markdownSource() const
{
    return markdown_;
}

} // namespace awm
