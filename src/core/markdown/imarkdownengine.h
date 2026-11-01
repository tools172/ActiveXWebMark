#pragma once

#include <string>

#include "document/outline.h"

namespace awm {

/// Options controlling Markdown -> HTML rendering.
struct RenderOptions {
    bool enableSmartypants = true;   ///< SmartyPants punctuation substitution.
    bool enableFootnotes = false;    ///< Markdown Extra-style footnotes.
    bool enableAutolink = true;      ///< Autolink bare URLs.
    bool enableStrikethrough = true; ///< ~~strikethrough~~.
    bool enableSuperscript = false;  ///< A^B superscript.
};

/// Result of rendering Markdown to HTML.
struct RenderResult {
    std::string bodyHtml; ///< Body HTML (without a wrapping document template).
    Outline outline;      ///< Heading outline (same model the navigation uses).
};

/// Markdown rendering engine abstraction.
///
/// Implementations are stateless and pure, so they are safe to call from a
/// background thread (used for asynchronous rendering of large documents).
class IMarkdownEngine {
public:
    virtual ~IMarkdownEngine() = default;

    virtual RenderResult render(const std::string& markdown,
                                const RenderOptions& options) const = 0;
};

} // namespace awm
