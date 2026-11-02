#include "layout/geometry.h"

#include <algorithm>

namespace awm::ui {

namespace {

int clampValue(int value, int low, int high)
{
    if (high < low) {
        return low;
    }
    return std::max(low, std::min(value, high));
}

// Sum of tab widths in [begin, end) plus the gap between consecutive tabs.
int spanWidth(const std::vector<int>& widths, int begin, int end, int gap)
{
    int total = 0;
    for (int i = begin; i < end; ++i) {
        total += widths[static_cast<std::size_t>(i)];
        if (i + 1 < end) {
            total += gap;
        }
    }
    return total;
}

} // namespace

SplitLayout splitArea(const RectI& area, Orientation orientation, int firstSize,
                      int dividerThickness, int minFirst, int minSecond)
{
    SplitLayout layout;
    const int divider = std::max(0, dividerThickness);

    if (orientation == Orientation::Horizontal) {
        const int usable = area.w - divider;
        if (usable <= 0) {
            layout.first = area;
            return layout;
        }

        if (minFirst + minSecond <= usable) {
            firstSize = clampValue(firstSize, minFirst, usable - minSecond);
        } else {
            firstSize = usable / 2;
        }

        layout.first = RectI{area.x, area.y, firstSize, area.h};
        layout.divider = RectI{area.x + firstSize, area.y, divider, area.h};
        layout.second = RectI{area.x + firstSize + divider, area.y, usable - firstSize, area.h};
    } else {
        const int usable = area.h - divider;
        if (usable <= 0) {
            layout.first = area;
            return layout;
        }

        if (minFirst + minSecond <= usable) {
            firstSize = clampValue(firstSize, minFirst, usable - minSecond);
        } else {
            firstSize = usable / 2;
        }

        layout.first = RectI{area.x, area.y, area.w, firstSize};
        layout.divider = RectI{area.x, area.y + firstSize, area.w, divider};
        layout.second = RectI{area.x, area.y + firstSize + divider, area.w, usable - firstSize};
    }
    return layout;
}

NavLayout layoutWithNav(const RectI& client, bool visible, int desiredNav,
                        int dividerThickness, int minNav, int maxNav, int minContent)
{
    NavLayout layout;
    layout.content = client;
    if (!visible) {
        return layout;
    }

    const int divider = std::max(0, dividerThickness);
    const int usable = client.w - divider;
    if (usable <= 0) {
        return layout;
    }

    // Keep as much room for the document as possible; a very narrow window
    // collapses the nav instead of squeezing the content below minContent.
    const int maxAllowed = std::max(0, usable - std::max(0, minContent));
    int navWidth = clampValue(desiredNav, 0, maxAllowed);
    if (navWidth < minNav) {
        navWidth = std::min(minNav, maxAllowed);
    }
    if (maxNav > 0) {
        navWidth = std::min(navWidth, maxNav);
    }
    navWidth = std::min(navWidth, usable);

    if (navWidth <= 0) {
        return layout;
    }

    layout.content = RectI{client.x, client.y, usable - navWidth, client.h};
    layout.divider = RectI{client.x + layout.content.w, client.y, divider, client.h};
    layout.nav = RectI{client.x + layout.content.w + divider, client.y, navWidth, client.h};
    layout.dividerVisible = divider > 0;
    return layout;
}

TabStripLayout layoutTabs(const RectI& area, const std::vector<int>& widths, int gap,
                          int padding, int closeSize, int firstVisible)
{
    TabStripLayout layout;
    layout.firstVisible = clampTabScroll(firstVisible, widths);
    if (widths.empty()) {
        return layout;
    }

    const int close = std::max(0, closeSize);
    const int left = area.x + std::max(0, padding);
    const int right = area.right() - std::max(0, padding);

    int x = left;
    layout.slots.reserve(widths.size());
    for (std::size_t i = 0; i < widths.size(); ++i) {
        const int width = std::max(0, widths[i]);
        TabSlot slot;
        if (static_cast<int>(i) < layout.firstVisible) {
            layout.slots.push_back(slot); // Scrolled out to the left.
            continue;
        }

        if (x + width > right) {
            layout.slots.push_back(slot); // Scrolled out to the right.
            x += width + gap;
            continue;
        }

        slot.visible = true;
        slot.rect = RectI{x, area.y, width, area.h};
        if (close > 0 && close < width) {
            const int closeRight = slot.rect.right() - std::max(2, padding / 2);
            slot.closeRect = RectI{closeRight - close,
                                   area.y + (area.h - close) / 2, close, close};
        }
        layout.slots.push_back(slot);
        x += width + gap;
    }
    return layout;
}

int clampTabScroll(int firstVisible, const std::vector<int>& widths)
{
    if (widths.empty()) {
        return 0;
    }
    return clampValue(firstVisible, 0, static_cast<int>(widths.size()) - 1);
}

int scrollToShow(int active, int firstVisible, int availableWidth,
                 const std::vector<int>& widths, int gap, int padding)
{
    if (widths.empty()) {
        return 0;
    }

    active = clampValue(active, 0, static_cast<int>(widths.size()) - 1);
    int visible = clampTabScroll(firstVisible, widths);
    if (active < visible) {
        visible = active;
    }

    const int rightLimit = availableWidth - std::max(0, padding);
    const int left = std::max(0, padding);
    while (visible < active) {
        const int x = left + spanWidth(widths, visible, active, gap);
        if (x + std::max(0, widths[static_cast<std::size_t>(active)]) <= rightLimit) {
            break;
        }
        ++visible;
    }
    return visible;
}

} // namespace awm::ui
