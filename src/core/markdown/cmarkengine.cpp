#include "markdown/cmarkengine.h"

#include <cmark.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

#include "document/headingparser.h"

namespace awm {

namespace {

// Collects the plain-text content of a node (used to render heading text).
void collectText(cmark_node* node, std::string& out)
{
    for (cmark_node* child = cmark_node_first_child(node); child != nullptr;
         child = cmark_node_next(child)) {
        switch (cmark_node_get_type(child)) {
        case CMARK_NODE_TEXT:
        case CMARK_NODE_CODE:
            if (const char* literal = cmark_node_get_literal(child)) {
                out += literal;
            }
            break;
        case CMARK_NODE_SOFTBREAK:
        case CMARK_NODE_LINEBREAK:
            out += ' ';
            break;
        default:
            collectText(child, out); // inline containers: strong, emph, link, ...
            break;
        }
    }
}

// Walks the parsed document and collects headings (level, source line, text).
Outline extractOutline(cmark_node* document)
{
    Outline outline;
    cmark_iter* iter = cmark_iter_new(document);
    cmark_event_type event;
    while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (event != CMARK_EVENT_ENTER) {
            continue;
        }
        cmark_node* node = cmark_iter_get_node(iter);
        if (cmark_node_get_type(node) != CMARK_NODE_HEADING) {
            continue;
        }

        OutlineItem item;
        item.level = cmark_node_get_heading_level(node);
        item.line = cmark_node_get_start_line(node);
        collectText(node, item.text);
        item.anchor = slugifyAnchor(item.text);
        outline.push_back(std::move(item));
    }
    cmark_iter_free(iter);
    return outline;
}

// Adds id="<anchor>" to each <h1>..<h6> opening tag, in document order, so the
// navigation tree anchors can locate the headings.
std::string injectHeadingIds(const std::string& html, const Outline& outline)
{
    std::string result;
    result.reserve(html.size() + outline.size() * 20);

    std::size_t pos = 0;
    std::size_t headingIndex = 0;
    while (true) {
        const std::size_t tag = html.find("<h", pos);
        if (tag == std::string::npos) {
            result.append(html, pos, std::string::npos);
            break;
        }

        // A heading opening tag is exactly "<h" + [1-6] + ">".
        if (tag + 3 < html.size() && html[tag + 1] == 'h' &&
            html[tag + 2] >= '1' && html[tag + 2] <= '6' && html[tag + 3] == '>') {
            result.append(html, pos, tag - pos);
            result.push_back('<');
            result.push_back('h');
            result.push_back(html[tag + 2]);
            if (headingIndex < outline.size()) {
                result += " id=\"";
                result += outline[headingIndex].anchor;
                result += "\"";
            }
            result.push_back('>');
            ++headingIndex;
            pos = tag + 4;
        } else {
            result.append(html, pos, tag - pos + 2);
            pos = tag + 2;
        }
    }
    return result;
}

} // namespace

RenderResult CmarkEngine::render(const std::string& markdown, const RenderOptions& /*options*/) const
{
    RenderResult result;

    cmark_parser* parser =
        cmark_parser_new(CMARK_OPT_SOURCEPOS | CMARK_OPT_UNSAFE);
    cmark_parser_feed(parser, markdown.data(), markdown.size());
    cmark_node* document = cmark_parser_finish(parser);

    if (document != nullptr) {
        result.outline = extractOutline(document);
        makeAnchorsUnique(result.outline);

        char* html = cmark_render_html(document, CMARK_OPT_UNSAFE);
        if (html != nullptr) {
            result.bodyHtml = injectHeadingIds(html, result.outline);
            std::free(html);
        }
        cmark_node_free(document);
    }

    cmark_parser_free(parser);
    return result;
}

} // namespace awm
