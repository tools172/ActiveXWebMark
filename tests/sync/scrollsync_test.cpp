#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ieditorview.h"
#include "ipreviewview.h"
#include "scrollsync.h"

using namespace awm;

namespace {

class FakeEditor : public IEditorView {
public:
    bool Create(HWND, const RECT&) override { return true; }
    HWND hwnd() const override { return nullptr; }
    void HandleHostNotification(LPNMHDR) override {}
    void SetBounds(const RECT&) override {}
    void Show(bool) override {}
    void SetText(const std::string&) override {}
    std::string GetText() const override { return {}; }
    void ApplyHighlighting(const std::string&, const std::vector<StyleSpan>&) override {}
    void GotoLine(int) override {}
    int FirstVisibleLine() const override { return firstVisibleLine; }
    void SetFirstVisibleLine(int line) override { setFirstVisibleLineCalls.push_back(line); }
    void SetScrollCallback(std::function<void(int)>) override {}
    void SetReadOnly(bool) override {}
    void SetDpiScale(double) override {}
    void Focus() override {}
    void SetChangeCallback(std::function<void()>) override {}

    int firstVisibleLine = 1;
    std::vector<int> setFirstVisibleLineCalls;
};

class FakePreview : public IPreviewView {
public:
    bool Create(HWND, const RECT&) override { return true; }
    void Destroy() override {}
    void SetBounds(const RECT&) override {}
    void Show(bool) override {}
    void SetHtml(const std::string&, const std::string&) override {}
    void SetZoom(int) override {}
    void ScrollToAnchor(const std::string&) override {}
    void ScrollToSourceLine(double line) override { scrollToSourceLineCalls.push_back(line); }
    bool SupportsSourceMapping() const override { return true; }
    bool RunJavaScript(const std::string&) override { return true; }
    void SetCallbacks(PreviewCallbacks) override {}
    bool TranslateAccelerator(MSG*) override { return false; }
    bool SupportsJavaScript() const override { return true; }
    void Focus() override {}

    std::vector<double> scrollToSourceLineCalls;
};

class ScrollSyncControllerTest : public ::testing::Test {
protected:
    void Configure(ScrollSyncController& controller)
    {
        controller.SetPreview(&preview_);
        controller.SetScheduler([this] { ++scheduleCalls_; }, [this] { ++cancelCalls_; });
    }

    FakeEditor editor_;
    FakePreview preview_;
    std::uint64_t now_ = 1000;
    int scheduleCalls_ = 0;
    int cancelCalls_ = 0;
};

} // namespace

TEST_F(ScrollSyncControllerTest, EditorScrollAppliesLastLineOnTick)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnEditorScrolled(10);
    controller.OnEditorScrolled(20);
    EXPECT_EQ(scheduleCalls_, 2);
    EXPECT_TRUE(preview_.scrollToSourceLineCalls.empty());

    controller.OnTimerTick();
    ASSERT_EQ(preview_.scrollToSourceLineCalls.size(), 1u);
    EXPECT_DOUBLE_EQ(preview_.scrollToSourceLineCalls[0], 20.0);
}

TEST_F(ScrollSyncControllerTest, PreviewScrollMovesEditor)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnPreviewScrolled(42.4);
    controller.OnTimerTick();

    ASSERT_EQ(editor_.setFirstVisibleLineCalls.size(), 1u);
    EXPECT_EQ(editor_.setFirstVisibleLineCalls[0], 42);
}

TEST_F(ScrollSyncControllerTest, PreviewEchoOfEditorScrollIsIgnored)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnEditorScrolled(10);
    controller.OnTimerTick(); // Drives the preview; its event is an echo.
    const int scheduledBefore = scheduleCalls_;

    controller.OnPreviewScrolled(10.0);
    EXPECT_EQ(scheduleCalls_, scheduledBefore);

    now_ += 300;
    controller.OnPreviewScrolled(11.0);
    EXPECT_EQ(scheduleCalls_, scheduledBefore + 1);
}

TEST_F(ScrollSyncControllerTest, EditorEchoOfPreviewScrollIsIgnored)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnPreviewScrolled(20.0);
    controller.OnTimerTick(); // Drives the editor; its event is an echo.
    const int scheduledBefore = scheduleCalls_;

    controller.OnEditorScrolled(20);
    EXPECT_EQ(scheduleCalls_, scheduledBefore);

    now_ += 300;
    controller.OnEditorScrolled(21);
    EXPECT_EQ(scheduleCalls_, scheduledBefore + 1);
}

TEST_F(ScrollSyncControllerTest, DisableCancelsAndEnableAlignsPreview)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    editor_.firstVisibleLine = 7;
    controller.SetEnabled(false);
    EXPECT_FALSE(controller.enabled());
    EXPECT_EQ(cancelCalls_, 1);

    controller.OnEditorScrolled(9);
    EXPECT_EQ(scheduleCalls_, 0);

    controller.SetEnabled(true);
    EXPECT_EQ(scheduleCalls_, 1);
    controller.OnTimerTick();

    ASSERT_EQ(preview_.scrollToSourceLineCalls.size(), 1u);
    EXPECT_DOUBLE_EQ(preview_.scrollToSourceLineCalls[0], 7.0);
}

TEST_F(ScrollSyncControllerTest, SuspendIgnoresBothPanesUntilExpired)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.Suspend(300);
    EXPECT_EQ(cancelCalls_, 1);

    controller.OnEditorScrolled(10);
    controller.OnPreviewScrolled(20);
    EXPECT_EQ(scheduleCalls_, 0);

    now_ += 301;
    controller.OnEditorScrolled(30);
    EXPECT_EQ(scheduleCalls_, 1);
}

TEST_F(ScrollSyncControllerTest, CancelDropsPendingWork)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnEditorScrolled(10);
    EXPECT_EQ(scheduleCalls_, 1);

    controller.Cancel();
    EXPECT_EQ(cancelCalls_, 1);

    controller.OnTimerTick();
    EXPECT_TRUE(preview_.scrollToSourceLineCalls.empty());
}

TEST_F(ScrollSyncControllerTest, ClearingPreviewCancelsAndDisablesSync)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnEditorScrolled(10);
    controller.SetPreview(nullptr);
    EXPECT_EQ(cancelCalls_, 1);

    controller.OnEditorScrolled(20);
    EXPECT_EQ(scheduleCalls_, 1);
}

TEST_F(ScrollSyncControllerTest, TickWithoutPendingWorkDoesNothing)
{
    ScrollSyncController controller(editor_, [this] { return now_; });
    Configure(controller);

    controller.OnTimerTick();
    EXPECT_TRUE(preview_.scrollToSourceLineCalls.empty());
    EXPECT_TRUE(editor_.setFirstVisibleLineCalls.empty());
}
