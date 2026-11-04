#pragma once

namespace awm::cmd {

// File commands.
constexpr int kFileOpen = 1001;
constexpr int kFileSave = 1002;
constexpr int kFileSaveAs = 1003;
constexpr int kFileCloseTab = 1004;
constexpr int kFileExit = 1005;
constexpr int kFileSetDefault = 1006;

// View commands.
constexpr int kViewEditor = 1010;
constexpr int kViewNav = 1011;
constexpr int kViewSplitSideBySide = 1012;
constexpr int kViewSplitStacked = 1013;
constexpr int kViewToggleSplit = 1014;
constexpr int kViewScrollSync = 1015;

// Theme selection. IDs are contiguous so menu radio checks can use a range.
// The block starts after the edit commands (1020-1029) and leaves room for
// future themes before the help commands (1100+).
constexpr int kViewThemeBase = 1030;
constexpr int kViewThemeMax = 64;

// Edit commands.
constexpr int kEditFind = 1020;
constexpr int kEditFindNext = 1021;
constexpr int kEditFindPrevious = 1022;

// Help commands.
constexpr int kHelpCheckUpdates = 1101;
constexpr int kHelpAbout = 1100;

static_assert(kViewThemeBase > kEditFindPrevious, "Theme IDs collide with edit commands");
static_assert(kViewThemeBase + kViewThemeMax - 1 < kHelpAbout,
              "Theme IDs collide with help commands");
static_assert(kViewThemeMax >= 53, "Theme command range must hold the full catalog");

// Recent-file menu commands. IDs 3000..3019 are the first page.
constexpr int kRecentBase = 3000;
constexpr int kRecentCount = 20;
constexpr int kRecentMore = 3021;

} // namespace awm::cmd
