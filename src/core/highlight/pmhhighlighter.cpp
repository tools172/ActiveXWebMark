#include "highlight/pmhhighlighter.h"

extern "C" {
#include <pmh_definitions.h>
#include <pmh_parser.h>
}

#include <algorithm>
#include <cstdint>
#include <vector>

namespace awm {

namespace {

MarkupStyle toMarkupStyle(pmh_element_type type)
{
    switch (type) {
    case pmh_H1:
    case pmh_H2:
    case pmh_H3:
    case pmh_H4:
    case pmh_H5:
    case pmh_H6:
        return MarkupStyle::Heading;
    case pmh_EMPH:
        return MarkupStyle::Emphasis;
    case pmh_STRONG:
        return MarkupStyle::Strong;
    case pmh_CODE:
        return MarkupStyle::Code;
    case pmh_VERBATIM:
        return MarkupStyle::CodeBlock;
    case pmh_LINK:
    case pmh_AUTO_LINK_URL:
    case pmh_AUTO_LINK_EMAIL:
        return MarkupStyle::Link;
    case pmh_IMAGE:
        return MarkupStyle::Image;
    case pmh_LIST_BULLET:
    case pmh_LIST_ENUMERATOR:
        return MarkupStyle::ListMarker;
    case pmh_BLOCKQUOTE:
        return MarkupStyle::BlockQuote;
    case pmh_HTML:
    case pmh_HTMLBLOCK:
    case pmh_HTML_ENTITY:
        return MarkupStyle::Html;
    case pmh_HRULE:
        return MarkupStyle::Hrule;
    case pmh_COMMENT:
        return MarkupStyle::Comment;
    case pmh_REFERENCE:
    case pmh_NOTE:
        return MarkupStyle::Reference;
    default:
        return MarkupStyle::Normal;
    }
}

} // namespace

std::vector<StyleSpan> PmhSyntaxHighlighter::highlight(const std::string& markdown)
{
    std::vector<StyleSpan> spans;

    // PMH parses a null-terminated buffer and may mutate it.
    std::vector<char> buffer(markdown.begin(), markdown.end());
    buffer.push_back('\0');

    pmh_element** elements = nullptr;
    pmh_markdown_to_elements(buffer.data(), pmh_EXT_NONE, &elements);

    // PMH reports offsets in Unicode code points; Scintilla and StyleSpan use
    // UTF-8 byte offsets. Build the mapping once: converting each element by
    // rescanning from the start is O(elements * document) and hangs on large
    // files with many emphasis/link elements.
    const bool ascii = std::all_of(markdown.begin(), markdown.end(), [](char ch) {
        return static_cast<unsigned char>(ch) < 0x80;
    });
    std::vector<std::uint32_t> byteOffsets;
    if (!ascii) {
        byteOffsets.reserve(markdown.size() + 1);
        std::size_t offset = 0;
        while (offset < markdown.size()) {
            byteOffsets.push_back(static_cast<std::uint32_t>(offset));
            const unsigned char c = static_cast<unsigned char>(markdown[offset]);
            std::size_t step = 1;
            if ((c & 0xE0) == 0xC0) {
                step = 2;
            } else if ((c & 0xF0) == 0xE0) {
                step = 3;
            } else if ((c & 0xF8) == 0xF0) {
                step = 4;
            }
            offset = std::min(markdown.size(), offset + step);
        }
        byteOffsets.push_back(static_cast<std::uint32_t>(markdown.size()));
    }

    const auto toByteOffset = [&](unsigned long codePointOffset) -> std::size_t {
        if (ascii) {
            return static_cast<std::size_t>(codePointOffset);
        }
        const std::size_t cp = static_cast<std::size_t>(codePointOffset);
        return cp < byteOffsets.size() ? static_cast<std::size_t>(byteOffsets[cp])
                                       : markdown.size();
    };

    for (int i = 0; i < pmh_NUM_LANG_TYPES; ++i) {
        const MarkupStyle style = toMarkupStyle(static_cast<pmh_element_type>(i));
        if (style == MarkupStyle::Normal) {
            continue;
        }

        for (const pmh_element* e = elements[i]; e != nullptr; e = e->next) {
            const std::size_t begin = toByteOffset(e->pos);
            const std::size_t end = toByteOffset(e->end);
            if (end <= begin) {
                continue;
            }

            // Trim trailing line breaks so the style does not cover them.
            std::size_t length = end - begin;
            while (length > 0 &&
                   (markdown[begin + length - 1] == '\n' || markdown[begin + length - 1] == '\r')) {
                --length;
            }
            if (length == 0) {
                continue;
            }

            spans.push_back({begin, length, style});
        }
    }

    pmh_free_elements(elements);
    return spans;
}

} // namespace awm
