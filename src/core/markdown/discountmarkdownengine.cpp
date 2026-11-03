#include "markdown/discountmarkdownengine.h"

extern "C" {
#include <mkdio.h>
}

#include "document/headingparser.h"

namespace awm {

namespace {

// Builds a Discount flag blob from render options.
mkd_flag_t* makeFlags(const RenderOptions& options)
{
    mkd_flag_t* flags = mkd_flags();
    mkd_set_flag_num(flags, MKD_TOC);
    mkd_set_flag_num(flags, MKD_NOSTYLE);
    mkd_set_flag_num(flags, MKD_IDANCHOR); // emit id= on headings for TOC navigation
    if (options.enableAutolink) {
        mkd_set_flag_num(flags, MKD_AUTOLINK);
    }
    if (!options.enableStrikethrough) {
        mkd_set_flag_num(flags, MKD_NOSTRIKETHROUGH);
    }
    if (!options.enableSmartypants) {
        mkd_set_flag_num(flags, MKD_NOPANTS);
    }
    if (options.enableFootnotes) {
        mkd_set_flag_num(flags, MKD_EXTRA_FOOTNOTE);
    }
    if (!options.enableSuperscript) {
        mkd_set_flag_num(flags, MKD_NOSUPERSCRIPT);
    }
    return flags;
}

} // namespace

RenderResult DiscountMarkdownEngine::render(const std::string& markdown,
                                            const RenderOptions& options) const
{
    RenderResult result;
    result.outline = extractHeadings(markdown);
    makeAnchorsUnique(result.outline);

    if (markdown.empty()) {
        return result;
    }

    // Discount requires the input to end with a line break.
    std::string text = markdown;
    if (text.back() != '\n') {
        text.push_back('\n');
    }

    mkd_flag_t* flags = makeFlags(options);
    MMIOT* doc = mkd_string(text.data(), static_cast<int>(text.size()), flags);
    if (doc != nullptr) {
        if (mkd_compile(doc, flags)) {
            char* out = nullptr;
            const int size = mkd_document(doc, &out);
            if (out != nullptr && size > 0) {
                result.bodyHtml.assign(out, static_cast<std::size_t>(size));
            }
        }
        mkd_cleanup(doc);
    }
    mkd_free_flags(flags);

    return result;
}

} // namespace awm
