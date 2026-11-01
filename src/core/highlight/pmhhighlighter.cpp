#include "highlight/pmhhighlighter.h"

extern "C" {
#include <pmh_definitions.h>
#include <pmh_parser.h>
}

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

// PMH reports offsets in Unicode code points; Scintilla (and our StyleSpan
// contract) uses UTF-8 byte offsets. Convert between the two.
std::size_t codePointToByteOffset(const std::string& text, std::size_t codePointOffset)
{
    std::size_t byteOffset = 0;
    for (std::size_t cp = 0; cp < codePointOffset && byteOffset < text.size(); ++cp) {
        const unsigned char c = static_cast<unsigned char>(text[byteOffset]);
        if (c < 0x80) {
            byteOffset += 1;
        } else if ((c & 0xE0) == 0xC0) {
            byteOffset += 2;
        } else if ((c & 0xF0) == 0xE0) {
            byteOffset += 3;
        } else if ((c & 0xF8) == 0xF0) {
            byteOffset += 4;
        } else {
            byteOffset += 1; // invalid byte; treat as a single code unit
        }
    }
    return byteOffset;
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

    for (int i = 0; i < pmh_NUM_LANG_TYPES; ++i) {
        const MarkupStyle style = toMarkupStyle(static_cast<pmh_element_type>(i));
        if (style == MarkupStyle::Normal) {
            continue;
        }

        for (const pmh_element* e = elements[i]; e != nullptr; e = e->next) {
            const std::size_t begin = codePointToByteOffset(markdown, e->pos);
            const std::size_t end = codePointToByteOffset(markdown, e->end);
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
