#pragma once

#include <vector>

namespace awm::ui {

/// One row of the navigation tree, derived from flat heading levels.
struct NavRow {
    int index = 0;      ///< Flat outline index this row represents.
    int depth = 0;      ///< Visual nesting depth (0 = root).
    int parent = -1;    ///< Parent row index, or -1 for a root row.
    bool hasChildren = false;
};

/// Builds a real parent/child hierarchy from heading levels.
///
/// A skipped level (for example H1 followed by H3) becomes one level deeper
/// than its nearest shallower ancestor; going back to a shallower level pops
/// all deeper headings. The returned order matches the input order.
std::vector<NavRow> buildNavRows(const std::vector<int>& levels);

} // namespace awm::ui
