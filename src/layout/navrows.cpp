#include "layout/navrows.h"

#include <algorithm>

namespace awm::ui {

std::vector<NavRow> buildNavRows(const std::vector<int>& levels)
{
    std::vector<NavRow> rows;
    rows.reserve(levels.size());

    // Stack of row indices whose heading levels strictly increase.
    std::vector<int> stack;
    stack.reserve(levels.size());

    for (std::size_t i = 0; i < levels.size(); ++i) {
        const int level = std::clamp(levels[i], 1, 6);

        while (!stack.empty() && levels[static_cast<std::size_t>(stack.back())] >= level) {
            stack.pop_back();
        }

        NavRow row;
        row.index = static_cast<int>(i);
        row.depth = static_cast<int>(stack.size());
        row.parent = stack.empty() ? -1 : stack.back();
        if (row.parent >= 0) {
            rows[static_cast<std::size_t>(row.parent)].hasChildren = true;
        }
        rows.push_back(row);
        stack.push_back(static_cast<int>(i));
    }
    return rows;
}

} // namespace awm::ui
