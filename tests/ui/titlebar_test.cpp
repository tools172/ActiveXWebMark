#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include <Windows.h>

#include "controls/titlebar.h"
#include "theme.h"

using namespace awm::ui;

namespace {

std::vector<MenuLabel> sampleMenus()
{
    return {
        parseMenuLabel(L"文件(&F)"),
        parseMenuLabel(L"编辑(&E)"),
        parseMenuLabel(L"视图(&V)"),
        parseMenuLabel(L"帮助(&H)"),
    };
}

POINT center(const RectI& rect)
{
    return POINT{rect.x + rect.w / 2, rect.y + rect.h / 2};
}

/// Simple top-down 32-bit DIB canvas for pixel assertions.
class Canvas {
public:
    Canvas(int width, int height) : width_(width), height_(height)
    {
        dc_ = ::CreateCompatibleDC(nullptr);
        if (dc_ == nullptr) {
            return;
        }

        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height; // Top-down rows.
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        bitmap_ = ::CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &bits_, nullptr, 0);
        if (bitmap_ != nullptr) {
            oldBitmap_ = ::SelectObject(dc_, bitmap_);
        }
        clear(RGB(255, 0, 255));
    }

    ~Canvas()
    {
        if (dc_ != nullptr && oldBitmap_ != nullptr) {
            ::SelectObject(dc_, oldBitmap_);
        }
        if (bitmap_ != nullptr) {
            ::DeleteObject(bitmap_);
        }
        if (dc_ != nullptr) {
            ::DeleteDC(dc_);
        }
    }

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    bool valid() const { return dc_ != nullptr && bitmap_ != nullptr; }
    HDC dc() const { return dc_; }

    void clear(COLORREF color)
    {
        HBRUSH brush = ::CreateSolidBrush(color);
        RECT rect{0, 0, width_, height_};
        ::FillRect(dc_, &rect, brush);
        ::DeleteObject(brush);
    }

    COLORREF pixel(int x, int y) const { return ::GetPixel(dc_, x, y); }

    int differentPixels(const RectI& rect, COLORREF background) const
    {
        int count = 0;
        for (int y = rect.y; y < rect.bottom(); ++y) {
            for (int x = rect.x; x < rect.right(); ++x) {
                if (pixel(x, y) != background) {
                    ++count;
                }
            }
        }
        return count;
    }

private:
    int width_ = 0;
    int height_ = 0;
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* bits_ = nullptr;
};

class TitleBarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        theme_.initialize(96);
        bar_.SetTheme(&theme_);
        bar_.SetMenus(sampleMenus());
        bar_.SetTitle(L"doc.md - ActiveXWebMark");
        bar_.Layout(1000);
    }

    Theme theme_;
    TitleBar bar_;
};

} // namespace

TEST_F(TitleBarTest, ExposesMenuMetadata)
{
    EXPECT_EQ(bar_.menuCount(), 4);
    EXPECT_EQ(bar_.menuAt(0).text, L"文件");
    EXPECT_EQ(bar_.menuAt(0).mnemonic, L'F');
    EXPECT_EQ(bar_.mnemonicMenu(L'f'), 0);
    EXPECT_EQ(bar_.mnemonicMenu(L'H'), 3);
    EXPECT_EQ(bar_.mnemonicMenu(L'z'), -1);
    EXPECT_EQ(bar_.mnemonicMenu(0), -1);
    EXPECT_EQ(bar_.menuAt(-1).text, L"");
    EXPECT_EQ(bar_.menuAt(99).text, L"");
}

TEST_F(TitleBarTest, LayoutUsesDpiScaledMetrics)
{
    EXPECT_EQ(bar_.height(), 36);
    const TitleBarLayout wide = bar_.layout();
    ASSERT_EQ(wide.menus.size(), 4u);
    EXPECT_EQ(wide.icon.w, 16);
    EXPECT_EQ(wide.icon.h, 16);
    EXPECT_EQ(wide.closeButton.w, 46);
    EXPECT_EQ(wide.closeButton.right(), 1000);
    EXPECT_GT(wide.menus[0].w, 20);

    theme_.setDpi(144);
    EXPECT_EQ(bar_.Layout(1000), 54);
    const TitleBarLayout scaled = bar_.layout();
    EXPECT_EQ(scaled.bar.h, 54);
    EXPECT_EQ(scaled.icon.w, 24);
    EXPECT_EQ(scaled.icon.h, 24);
    EXPECT_EQ(scaled.closeButton.w, 69);
    EXPECT_EQ(scaled.closeButton.right(), 1000);
    EXPECT_GE(scaled.menus[0].w, wide.menus[0].w);
}

TEST_F(TitleBarTest, HoverTracksMenusAndButtons)
{
    const TitleBarLayout layout = bar_.layout();
    const POINT menuPoint = center(layout.menus[1]);
    EXPECT_TRUE(bar_.OnMouseMove(menuPoint));
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::Menu);
    EXPECT_EQ(bar_.hoveredMenu(), 1);
    EXPECT_FALSE(bar_.OnMouseMove(menuPoint));

    const POINT closePoint = center(layout.closeButton);
    EXPECT_TRUE(bar_.OnMouseMove(closePoint));
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::Close);
    EXPECT_EQ(bar_.hoveredMenu(), -1);

    EXPECT_TRUE(bar_.OnMouseLeave());
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::None);
    EXPECT_EQ(bar_.hoveredMenu(), -1);
    EXPECT_FALSE(bar_.OnMouseLeave());
}

TEST_F(TitleBarTest, SetHoverForTestClampsMenuIndex)
{
    bar_.SetHoverForTest(TitleBarHit::Menu, 2);
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::Menu);
    EXPECT_EQ(bar_.hoveredMenu(), 2);

    bar_.SetHoverForTest(TitleBarHit::Menu, 99);
    EXPECT_EQ(bar_.hoveredMenu(), 3);

    bar_.SetHoverForTest(TitleBarHit::Close, 2);
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::Close);
    EXPECT_EQ(bar_.hoveredMenu(), -1);

    bar_.SetHoverForTest(TitleBarHit::None, 2);
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::None);
    EXPECT_EQ(bar_.hoveredMenu(), -1);
}

TEST_F(TitleBarTest, MouseMoveOutsideClearsHover)
{
    const TitleBarLayout layout = bar_.layout();
    ASSERT_TRUE(bar_.OnMouseMove(center(layout.menus[0])));
    ASSERT_EQ(bar_.hoveredMenu(), 0);

    EXPECT_TRUE(bar_.OnMouseMove(POINT{10, 500}));
    EXPECT_EQ(bar_.hoveredPart(), TitleBarHit::None);
    EXPECT_EQ(bar_.hoveredMenu(), -1);
}

TEST_F(TitleBarTest, ClickReportsEachAction)
{
    const TitleBarLayout layout = bar_.layout();

    const auto click = [this](const RectI& rect) {
        const POINT point = center(rect);
        const bool changed = bar_.OnLButtonDown(point);
        EXPECT_TRUE(changed);
        EXPECT_TRUE(bar_.hasPressed());
        return bar_.OnLButtonUp(point);
    };

    TitleBarEvent event = click(layout.minimizeButton);
    EXPECT_EQ(event.action, TitleBarAction::Minimize);
    EXPECT_EQ(event.menuIndex, -1);

    event = click(layout.maximizeButton);
    EXPECT_EQ(event.action, TitleBarAction::ToggleMaximize);

    event = click(layout.closeButton);
    EXPECT_EQ(event.action, TitleBarAction::Close);

    event = click(layout.icon);
    EXPECT_EQ(event.action, TitleBarAction::ShowSystemMenu);

    event = click(layout.menus[2]);
    EXPECT_EQ(event.action, TitleBarAction::OpenMenu);
    EXPECT_EQ(event.menuIndex, 2);

    EXPECT_FALSE(bar_.hasPressed());
}

TEST_F(TitleBarTest, ReleasingOverAnotherPartCancelsAction)
{
    const TitleBarLayout layout = bar_.layout();
    ASSERT_TRUE(bar_.OnLButtonDown(center(layout.menus[0])));
    const TitleBarEvent event = bar_.OnLButtonUp(center(layout.menus[1]));
    EXPECT_EQ(event.action, TitleBarAction::None);
    EXPECT_FALSE(bar_.hasPressed());
}

TEST_F(TitleBarTest, CaptionAndEmptyAreasDoNotPress)
{
    const TitleBarLayout layout = bar_.layout();
    EXPECT_FALSE(bar_.OnLButtonDown(POINT{layout.title.x + 10, 18}));
    EXPECT_FALSE(bar_.hasPressed());
    EXPECT_EQ(bar_.OnLButtonUp(POINT{layout.title.x + 10, 18}).action,
              TitleBarAction::None);

    EXPECT_FALSE(bar_.OnLButtonDown(POINT{10, 500}));
    EXPECT_FALSE(bar_.hasPressed());
}

TEST_F(TitleBarTest, CancelPressClearsState)
{
    const TitleBarLayout layout = bar_.layout();
    const POINT point = center(layout.closeButton);
    ASSERT_TRUE(bar_.OnLButtonDown(point));
    ASSERT_TRUE(bar_.hasPressed());
    bar_.CancelPress();
    EXPECT_FALSE(bar_.hasPressed());
    EXPECT_EQ(bar_.OnLButtonUp(point).action, TitleBarAction::None);
}

TEST_F(TitleBarTest, StateSettersClampIndexes)
{
    bar_.SetMenuMode(true);
    EXPECT_TRUE(bar_.menuMode());
    bar_.SetHighlightedMenu(99);
    EXPECT_EQ(bar_.highlightedMenu(), 3);
    bar_.SetHighlightedMenu(-5);
    EXPECT_EQ(bar_.highlightedMenu(), -1);
    bar_.SetOpenMenu(2);
    EXPECT_EQ(bar_.openMenu(), 2);
    bar_.SetOpenMenu(99);
    EXPECT_EQ(bar_.openMenu(), 3);
    bar_.SetOpenMenu(-1);
    EXPECT_EQ(bar_.openMenu(), -1);
    bar_.SetMenuMode(false);
    EXPECT_FALSE(bar_.menuMode());
}

TEST_F(TitleBarTest, MenuRectAndHitTestAgree)
{
    const TitleBarLayout layout = bar_.layout();
    for (int i = 0; i < bar_.menuCount(); ++i) {
        const RECT rect = bar_.menuRect(i);
        const RectI& expected = layout.menus[static_cast<std::size_t>(i)];
        EXPECT_EQ(rect.left, expected.x);
        EXPECT_EQ(rect.top, expected.y);
        EXPECT_EQ(rect.right, expected.right());
        EXPECT_EQ(rect.bottom, expected.bottom());

        int index = -1;
        EXPECT_EQ(bar_.hitTest(POINT{(rect.left + rect.right) / 2,
                                     (rect.top + rect.bottom) / 2}, &index),
                  TitleBarHit::Menu);
        EXPECT_EQ(index, i);
    }
    const RECT empty = bar_.menuRect(99);
    EXPECT_EQ(empty.right - empty.left, 0);
    EXPECT_EQ(empty.bottom - empty.top, 0);
}

TEST_F(TitleBarTest, PaintFillsBarAndLeavesRestUntouched)
{
    Canvas canvas(1000, 80);
    ASSERT_TRUE(canvas.valid());
    bar_.Paint(canvas.dc());

    EXPECT_EQ(canvas.pixel(5, 5), RGB(248, 249, 251));       // titleBarBg
    EXPECT_EQ(canvas.pixel(500, 35), RGB(225, 228, 232));    // divider
    EXPECT_EQ(canvas.pixel(500, 36), RGB(255, 0, 255));      // untouched
    EXPECT_EQ(canvas.pixel(500, 70), RGB(255, 0, 255));      // untouched

    bar_.SetActive(false);
    bar_.Paint(canvas.dc());
    EXPECT_EQ(canvas.pixel(5, 5), RGB(251, 252, 253));       // titleBarInactive
}

TEST_F(TitleBarTest, PaintDrawsMenuAndButtonHoverStates)
{
    Canvas canvas(1000, 60);
    ASSERT_TRUE(canvas.valid());
    const TitleBarLayout layout = bar_.layout();

    // Menu hover.
    ASSERT_TRUE(bar_.OnMouseMove(center(layout.menus[1])));
    bar_.Paint(canvas.dc());
    const RectI& menu = layout.menus[1];
    EXPECT_EQ(canvas.pixel(menu.x + 3, menu.y + 18), RGB(237, 239, 242));

    // Window button hover.
    ASSERT_TRUE(bar_.OnMouseMove(center(layout.minimizeButton)));
    bar_.Paint(canvas.dc());
    EXPECT_EQ(canvas.pixel(layout.minimizeButton.x + 5, 5), RGB(231, 234, 238));

    // Close button hover turns red.
    ASSERT_TRUE(bar_.OnMouseMove(center(layout.closeButton)));
    bar_.Paint(canvas.dc());
    EXPECT_EQ(canvas.pixel(layout.closeButton.x + 5, 5), RGB(232, 17, 35));

    // Open menu uses the accent tint.
    bar_.SetOpenMenu(2);
    bar_.OnMouseLeave();
    bar_.Paint(canvas.dc());
    const RectI& open = layout.menus[2];
    EXPECT_EQ(canvas.pixel(open.x + 3, open.y + 18), RGB(232, 240, 254));
}

TEST_F(TitleBarTest, PaintDrawsGlyphsAndTitleText)
{
    Canvas canvas(1000, 60);
    ASSERT_TRUE(canvas.valid());
    bar_.Paint(canvas.dc());

    const TitleBarLayout layout = bar_.layout();
    // The minimize glyph is a horizontal 1px line through the button center.
    EXPECT_EQ(canvas.pixel(center(layout.minimizeButton).x,
                           center(layout.minimizeButton).y),
              RGB(31, 35, 40));

    // The title text must leave dark pixels inside the title rectangle.
    EXPECT_GT(canvas.differentPixels(layout.title, RGB(248, 249, 251)), 20);
}

TEST_F(TitleBarTest, MaximizedGlyphDiffersFromRestoreGlyph)
{
    Canvas canvas(1000, 60);
    ASSERT_TRUE(canvas.valid());
    const TitleBarLayout layout = bar_.layout();

    bar_.SetMaximized(false);
    bar_.Paint(canvas.dc());
    const int normal = canvas.differentPixels(layout.maximizeButton, RGB(248, 249, 251));

    bar_.SetMaximized(true);
    bar_.Paint(canvas.dc());
    const int maximized =
        canvas.differentPixels(layout.maximizeButton, RGB(248, 249, 251));

    EXPECT_GT(normal, 0);
    EXPECT_GT(maximized, 0);
    EXPECT_NE(normal, maximized);
}

TEST_F(TitleBarTest, DpiAffectsMenuWidths)
{
    theme_.setDpi(96);
    bar_.Layout(1000);
    const int normalWidth = bar_.layout().menus[0].w;

    theme_.setDpi(192);
    bar_.Layout(1000);
    const int scaledWidth = bar_.layout().menus[0].w;
    EXPECT_GT(scaledWidth, normalWidth);
}
