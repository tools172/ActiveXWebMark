#pragma once

#include <vector>

namespace awm::ui {

/// One rendered block and the source line range it came from.
///
/// `top` and `height` are document coordinates in CSS pixels, measured from
/// the rendered preview. `startLine`/`endLine` are 1-based, inclusive.
struct ScrollBlock {
    int startLine = 0;
    int endLine = 0;
    double top = 0.0;
    double height = 0.0;
};

/// Bidirectional mapping between Markdown source lines and preview pixels.
///
/// The map is built from `data-sourcepos` attributes emitted by cmark. Nested
/// blocks (for example `ul > li > p`) overlap; lookups choose the most
/// specific block: the smallest line span for line -> pixel, and the smallest
/// height for pixel -> line. Gaps between blocks are interpolated so blank
/// source lines and block margins do not cause jumps.
class ScrollMap {
public:
    ScrollMap() = default;
    explicit ScrollMap(std::vector<ScrollBlock> blocks);

    /// Replaces the map content. Invalid blocks are dropped and the rest are
    /// sorted by their document position.
    void rebuild(std::vector<ScrollBlock> blocks);

    bool empty() const { return blocks_.empty(); }
    const std::vector<ScrollBlock>& blocks() const { return blocks_; }

    /// Document y coordinate for a (possibly fractional) source line.
    double yForLine(double line) const;

    /// Source line (possibly fractional) at a document y coordinate.
    double lineForY(double y) const;

private:
    std::vector<ScrollBlock> blocks_;
};

} // namespace awm::ui
