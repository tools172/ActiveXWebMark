#pragma once

#include "highlight/isyntaxhighlighter.h"

namespace awm {

/// Syntax highlighter backed by PEG Markdown Highlight (PMH).
class PmhSyntaxHighlighter : public ISyntaxHighlighter {
public:
    std::vector<StyleSpan> highlight(const std::string& markdown) override;
};

} // namespace awm
