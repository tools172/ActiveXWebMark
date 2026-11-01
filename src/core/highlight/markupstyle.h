#pragma once

namespace awm {

/// Semantic Markdown element categories produced by a syntax highlighter.
///
/// The highlighter reports *what* a span is; the editor decides *how* to color
/// it. This decouples parsing from presentation (theme).
enum class MarkupStyle {
    Normal,
    Heading,    ///< ATX/setext heading.
    Emphasis,   ///< *italic*.
    Strong,     ///< **bold**.
    Code,       ///< `inline code`.
    CodeBlock,  ///< Fenced/indented code block.
    Link,       ///< [text](url) or autolink.
    Image,      ///< ![alt](url).
    ListMarker, ///< Bullet or enumerator of a list item.
    BlockQuote, ///< > quoted block.
    Html,       ///< Inline HTML or HTML block.
    Hrule,      ///< Horizontal rule (---).
    Comment,    ///< HTML comment.
    Reference,  ///< Reference definition / note.
};

} // namespace awm
