#include "layout/scrollmap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace awm::ui {

namespace {

bool isValid(const ScrollBlock& block)
{
    return block.startLine >= 1 && block.endLine >= block.startLine && block.height > 0.0 &&
           std::isfinite(block.top) && std::isfinite(block.height);
}

int lineSpan(const ScrollBlock& block)
{
    return block.endLine - block.startLine + 1;
}

double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

} // namespace

ScrollMap::ScrollMap(std::vector<ScrollBlock> blocks)
{
    rebuild(std::move(blocks));
}

void ScrollMap::rebuild(std::vector<ScrollBlock> blocks)
{
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [](const ScrollBlock& block) { return !isValid(block); }),
                 blocks.end());

    std::sort(blocks.begin(), blocks.end(), [](const ScrollBlock& a, const ScrollBlock& b) {
        if (a.top != b.top) {
            return a.top < b.top;
        }
        return a.height > b.height;
    });
    blocks_ = std::move(blocks);
}

double ScrollMap::yForLine(double line) const
{
    if (blocks_.empty()) {
        return 0.0;
    }

    // Deepest block containing the line: smallest source line span.
    const ScrollBlock* best = nullptr;
    int bestSpan = std::numeric_limits<int>::max();
    for (const ScrollBlock& block : blocks_) {
        if (line < static_cast<double>(block.startLine) ||
            line > static_cast<double>(block.endLine)) {
            continue;
        }
        const int span = lineSpan(block);
        if (span < bestSpan || (span == bestSpan && best != nullptr &&
                                block.height < best->height)) {
            best = &block;
            bestSpan = span;
        }
    }

    if (best != nullptr) {
        const double span = static_cast<double>(lineSpan(*best));
        const double fraction =
            clamp01((line - static_cast<double>(best->startLine)) / span);
        return best->top + fraction * best->height;
    }

    // The line is in a gap between blocks (blank source lines). Interpolate
    // vertically between the previous block's bottom and the next block's top.
    const ScrollBlock* previous = nullptr;
    const ScrollBlock* next = nullptr;
    for (const ScrollBlock& block : blocks_) {
        if (block.endLine < line &&
            (previous == nullptr || block.endLine > previous->endLine)) {
            previous = &block;
        }
        if (block.startLine > line &&
            (next == nullptr || block.startLine < next->startLine)) {
            next = &block;
        }
    }

    if (previous != nullptr && next != nullptr) {
        const double previousBottom = previous->top + previous->height;
        const double nextTop = next->top;
        const double lineGap = static_cast<double>(next->startLine - previous->endLine);
        if (lineGap > 0.0 && nextTop > previousBottom) {
            const double fraction =
                clamp01((line - static_cast<double>(previous->endLine)) / lineGap);
            return previousBottom + fraction * (nextTop - previousBottom);
        }
        return previousBottom;
    }
    if (previous != nullptr) {
        return previous->top + previous->height;
    }
    if (next != nullptr) {
        return next->top;
    }
    return 0.0;
}

double ScrollMap::lineForY(double y) const
{
    if (blocks_.empty()) {
        return 1.0;
    }

    // Deepest block containing y: smallest rendered height.
    const ScrollBlock* best = nullptr;
    for (const ScrollBlock& block : blocks_) {
        const double bottom = block.top + block.height;
        if (y < block.top || y > bottom) {
            continue;
        }
        if (best == nullptr || block.height < best->height ||
            (block.height == best->height && lineSpan(block) < lineSpan(*best))) {
            best = &block;
        }
    }

    if (best != nullptr) {
        const double fraction = clamp01((y - best->top) / best->height);
        const double span = static_cast<double>(lineSpan(*best));
        const double line = static_cast<double>(best->startLine) + fraction * span;
        return std::min(line, static_cast<double>(best->endLine));
    }

    // The y coordinate is in a vertical gap between blocks. Interpolate between
    // the previous block's last line and the next block's first line.
    const ScrollBlock* previous = nullptr;
    const ScrollBlock* next = nullptr;
    for (const ScrollBlock& block : blocks_) {
        const double bottom = block.top + block.height;
        if (bottom <= y && (previous == nullptr || bottom > previous->top + previous->height)) {
            previous = &block;
        }
        if (block.top >= y && (next == nullptr || block.top < next->top)) {
            next = &block;
        }
    }

    if (previous != nullptr && next != nullptr) {
        const double previousBottom = previous->top + previous->height;
        const double nextTop = next->top;
        const double lineGap = static_cast<double>(next->startLine - previous->endLine);
        if (nextTop > previousBottom && lineGap > 0.0) {
            const double fraction = clamp01((y - previousBottom) / (nextTop - previousBottom));
            return static_cast<double>(previous->endLine) + fraction * lineGap;
        }
        return static_cast<double>(previous->endLine);
    }
    if (previous != nullptr) {
        return static_cast<double>(previous->endLine);
    }
    if (next != nullptr) {
        return static_cast<double>(next->startLine);
    }
    return 1.0;
}

} // namespace awm::ui
