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
// The block lives between the edit commands and the recent-file block, and is
// large enough for the built-in catalog plus user-imported themes.
constexpr int kViewThemeBase = 1200;
constexpr int kViewThemeMax = 512;

// Opens the theme settings window.
constexpr int kViewThemeSettings = 1023;

// Edit commands.
constexpr int kEditFind = 1020;
constexpr int kEditFindNext = 1021;
constexpr int kEditFindPrevious = 1022;

// Help commands.
constexpr int kHelpCheckUpdates = 1101;
constexpr int kHelpAbout = 1100;

static_assert(kViewThemeSettings > kEditFindPrevious,
              "Theme settings ID collides with edit commands");
static_assert(kViewThemeSettings < kHelpAbout,
              "Theme settings ID collides with help commands");
static_assert(kViewThemeBase > kEditFindPrevious, "Theme IDs collide with edit commands");
static_assert(kViewThemeBase + kViewThemeMax - 1 <= 0xFFFF,
              "Theme IDs must fit in WM_COMMAND's WORD id");
static_assert(kViewThemeMax >= 53 + 459, "Theme command range must hold built-in and user themes");

// Recent-file menu commands. IDs 3000..3019 are the first page.
constexpr int kRecentBase = 3000;
constexpr int kRecentCount = 20;
constexpr int kRecentMore = 3021;

static_assert(kViewThemeBase + kViewThemeMax - 1 < kRecentBase,
              "Theme IDs collide with recent-file commands");

} // namespace awm::cmd
