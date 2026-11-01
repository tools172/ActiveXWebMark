#pragma once

#include "markdown/imarkdownengine.h"

namespace awm {

/// Markdown engine backed by the Discount C library.
class DiscountMarkdownEngine : public IMarkdownEngine {
public:
    RenderResult render(const std::string& markdown,
                        const RenderOptions& options) const override;
};

} // namespace awm
