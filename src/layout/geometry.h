#pragma once

#include <vector>

namespace awm::ui {

/// Device-independent integer rectangle used by the layout functions.
struct RectI {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    int left() const { return x; }
    int top() const { return y; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
};

/// Split axis. Horizontal means two panes side by side (left/right split);
/// Vertical means two panes stacked (top/bottom split).
enum class Orientation {
    Horizontal,
    Vertical,
};

/// Result of splitting an area into first pane, divider, second pane.
struct SplitLayout {
    RectI first;
    RectI divider;
    RectI second;
};

/// Splits `area` along `orientation`.
///
/// `firstSize` is the desired size of the first pane along the split axis.
/// It is clamped so both panes keep their minimum size when the area is large
/// enough; when it is not, the usable space is divided in half.
SplitLayout splitArea(const RectI& area, Orientation orientation, int firstSize,
                      int dividerThickness, int minFirst, int minSecond);

/// Right-hand navigation panel layout.
struct NavLayout {
    RectI content;
    RectI divider;
    RectI nav;
    bool dividerVisible = false;
};

/// Lays out `client` as content + divider + right nav when `visible`.
///
/// The navigation width is clamped to [minNav, maxNav] while keeping at least
/// `minContent` for the document area. If the window is too small to show a
/// usable nav panel, the nav (and divider) are hidden and content fills the
/// client area.
NavLayout layoutWithNav(const RectI& client, bool visible, int desiredNav,
                        int dividerThickness, int minNav, int maxNav, int minContent);

/// One tab slot computed by layoutTabs().
struct TabSlot {
    RectI rect;      ///< Empty when the tab is outside the visible strip.
    RectI closeRect; ///< Empty when the close button would not be drawn.
    bool visible = false;
};

struct TabStripLayout {
    std::vector<TabSlot> slots;
    int firstVisible = 0;
    int scrollOffset = 0; ///< Pixel offset of the first drawn tab (for animation).
};

/// Computes horizontal tab rectangles.
///
/// Tabs are drawn from `firstVisible` onward, separated by `gap`, starting at
/// `area.x + padding`. `closeSize` is the square close-button size reserved at
/// the right edge of each tab.
TabStripLayout layoutTabs(const RectI& area, const std::vector<int>& widths, int gap,
                          int padding, int closeSize, int firstVisible);

/// Clamps a first-visible tab index into [0, widths.size()-1] (0 when empty).
int clampTabScroll(int firstVisible, const std::vector<int>& widths);

/// Returns the smallest first-visible index that keeps `active` fully visible.
int scrollToShow(int active, int firstVisible, int availableWidth,
                 const std::vector<int>& widths, int gap, int padding);

/// Total width of all tabs plus the gaps between adjacent tabs.
int tabsTotalWidth(const std::vector<int>& widths, int gap);

/// Smallest first-visible index whose remaining tabs still fit in `available`
/// width (including `padding` on both sides).
int tabStripTailStart(const std::vector<int>& widths, int available, int gap, int padding);

/// Applies a one-step scroll delta, clamped to [0, maxFirstVisible].
int tabStripScrollTarget(int firstVisible, int delta, int maxFirstVisible);

/// Rectangle of the sidebar toggle at the right end of a tab strip.
RectI tabStripNavButtonRect(const RectI& client, int buttonSize, int margin);

/// Right edge of the tab area, left of the sidebar toggle by `gap`.
int tabStripAreaRight(int navButtonLeft, int gap);

/// Scroll-arrow rectangles at the left and right ends of the tab area.
RectI tabStripLeftArrowRect(int top, int bottom, int arrowWidth);
RectI tabStripRightArrowRect(int tabAreaRight, int top, int bottom, int arrowWidth);

/// Hit test with inclusive left/top and exclusive right/bottom edges.
bool rectContains(const RectI& rect, int x, int y);

} // namespace awm::ui
