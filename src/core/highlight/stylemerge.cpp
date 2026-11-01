#include "highlight/stylemerge.h"

#include <algorithm>

namespace awm {

std::vector<MarkupStyle> MergeStyleSpans(std::size_t length, const std::vector<StyleSpan>& spans)
{
    std::vector<MarkupStyle> styles(length, MarkupStyle::Normal);

    std::vector<StyleSpan> sorted(spans.begin(), spans.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const StyleSpan& a, const StyleSpan& b) { return a.offset < b.offset; });

    for (const StyleSpan& span : sorted) {
        const std::size_t begin = std::min(span.offset, length);
        const std::size_t end = std::min(span.offset + span.length, length);
        for (std::size_t i = begin; i < end; ++i) {
            styles[i] = span.style;
        }
    }
    return styles;
}

} // namespace awm
