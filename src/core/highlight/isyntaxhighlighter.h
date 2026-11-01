#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "highlight/markupstyle.h"

namespace awm {

/// A contiguous run of source text assigned a semantic style.
///
/// `offset` and `length` are UTF-8 byte offsets into the Markdown source.
struct StyleSpan {
    std::size_t offset = 0;
    std::size_t length = 0;
    MarkupStyle style = MarkupStyle::Normal;
};

/// Markdown syntax highlighter abstraction.
class ISyntaxHighlighter {
public:
    virtual ~ISyntaxHighlighter() = default;

    /// Parses Markdown and returns semantic style spans.
    virtual std::vector<StyleSpan> highlight(const std::string& markdown) = 0;
};

} // namespace awm
