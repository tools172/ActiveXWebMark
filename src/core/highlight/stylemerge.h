#pragma once

#include <cstddef>
#include <vector>

#include "highlight/isyntaxhighlighter.h"

namespace awm {

/// Merges possibly-overlapping style spans into a per-byte style array.
///
/// Every returned element covers one byte of the document. Overlapping spans
/// resolve to the span that starts later (inner/more specific wins); all
/// offsets are clamped to [0, length). This is the engine-independent core of
/// applying syntax highlighting, kept separate from the editor for testing.
std::vector<MarkupStyle> MergeStyleSpans(std::size_t length,
                                         const std::vector<StyleSpan>& spans);

} // namespace awm
