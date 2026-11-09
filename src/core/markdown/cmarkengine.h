#pragma once

#include "markdown/imarkdownengine.h"

namespace awm {

/// Markdown engine backed by the cmark-gfm parser (CommonMark + GFM tables).
///
/// Produces standards-compliant CommonMark HTML (fenced code blocks, lists
/// interrupting paragraphs, etc.), unlike Discount which is Markdown.pl-style.
class CmarkEngine : public IMarkdownEngine {
public:
    RenderResult render(const std::string& markdown,
                        const RenderOptions& options) const override;
};

} // namespace awm
