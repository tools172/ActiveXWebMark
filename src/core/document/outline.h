#pragma once

#include <string>
#include <vector>

namespace awm {

/// A single entry in a document outline (the navigation tree).
///
/// For Markdown, entries are ATX headings (H1..H6). For a future JSONL
/// (agent conversation) document, entries would be the conversation records.
struct OutlineItem {
    std::string text;    ///< Display text (heading text, markup stripped).
    std::string anchor;  ///< Anchor used to locate the section in the preview.
    int line = -1;       ///< 1-based source line number; -1 when unknown.
    int level = 1;       ///< Heading level (H1 = 1, ...).
    std::vector<OutlineItem> children;
};

using Outline = std::vector<OutlineItem>;

} // namespace awm
