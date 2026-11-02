#pragma once

namespace awm::cmd {

// File commands.
constexpr int kFileOpen = 1001;
constexpr int kFileSave = 1002;
constexpr int kFileSaveAs = 1003;
constexpr int kFileCloseTab = 1004;
constexpr int kFileExit = 1005;

// View commands.
constexpr int kViewEditor = 1010;
constexpr int kViewNav = 1011;
constexpr int kViewSplitSideBySide = 1012;
constexpr int kViewSplitStacked = 1013;
constexpr int kViewToggleSplit = 1014;

// Help commands.
constexpr int kHelpAbout = 1100;

// Recent-file menu commands. IDs 3000..3019 are the first page.
constexpr int kRecentBase = 3000;
constexpr int kRecentCount = 20;
constexpr int kRecentMore = 3021;
constexpr int kRecentClear = 3022;

} // namespace awm::cmd
