#include "scrollsync.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "ieditorview.h"
#include "ipreviewview.h"

namespace awm {

namespace {

// Time during which the pane that was just scrolled programmatically ignores
// its own scroll event, so the two panes cannot drive each other in a loop.
constexpr std::uint64_t kEchoSuppressMs = 250;

} // namespace

ScrollSyncController::ScrollSyncController(IEditorView& editor, Clock clock)
    : editor_(editor)
    , clock_(clock ? std::move(clock)
                   : Clock([] { return static_cast<std::uint64_t>(::GetTickCount64()); }))
{
}

void ScrollSyncController::SetPreview(IPreviewView* preview)
{
    if (preview_ == preview) {
        return;
    }
    Cancel();
    preview_ = preview;
}

void ScrollSyncController::SetScheduler(Schedule schedule, CancelSchedule cancel)
{
    schedule_ = std::move(schedule);
    cancelSchedule_ = std::move(cancel);
}

void ScrollSyncController::SetEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;
    if (enabled_) {
        AlignPreviewToEditor();
    } else {
        Cancel();
    }
}

void ScrollSyncController::OnEditorScrolled(int firstVisibleLine)
{
    if (!enabled_ || preview_ == nullptr || !schedule_) {
        return;
    }
    if (Now() < suppressEditorUntil_) {
        return; // Echo of a preview-driven editor scroll.
    }

    pendingPreviewLine_ = firstVisibleLine;
    pendingSource_ = Source::Editor;
    ScheduleApply();
}

void ScrollSyncController::OnPreviewScrolled(double sourceLine)
{
    if (!enabled_ || preview_ == nullptr || !schedule_) {
        return;
    }
    if (Now() < suppressPreviewUntil_) {
        return; // Echo of an editor-driven preview scroll.
    }

    pendingEditorLine_ = static_cast<int>(std::lround(sourceLine));
    pendingSource_ = Source::Preview;
    ScheduleApply();
}

void ScrollSyncController::OnTimerTick()
{
    const Source source = pendingSource_;
    pendingSource_ = Source::None;
    if (!enabled_) {
        return;
    }

    const std::uint64_t now = Now();
    if (source == Source::Editor && preview_ != nullptr) {
        suppressPreviewUntil_ = now + kEchoSuppressMs;
        preview_->ScrollToSourceLine(static_cast<double>(pendingPreviewLine_));
    } else if (source == Source::Preview) {
        suppressEditorUntil_ = now + kEchoSuppressMs;
        editor_.SetFirstVisibleLine(pendingEditorLine_);
    }
}

void ScrollSyncController::AlignPreviewToEditor()
{
    if (!enabled_ || preview_ == nullptr || !schedule_) {
        return;
    }

    pendingPreviewLine_ = editor_.FirstVisibleLine();
    pendingSource_ = Source::Editor;
    ScheduleApply();
}

void ScrollSyncController::Cancel()
{
    pendingSource_ = Source::None;
    if (cancelSchedule_) {
        cancelSchedule_();
    }
}

void ScrollSyncController::Suspend(std::uint64_t durationMs)
{
    const std::uint64_t until = Now() + durationMs;
    suppressEditorUntil_ = std::max(suppressEditorUntil_, until);
    suppressPreviewUntil_ = std::max(suppressPreviewUntil_, until);
    Cancel();
}

std::uint64_t ScrollSyncController::Now() const
{
    return clock_ ? clock_() : 0;
}

void ScrollSyncController::ScheduleApply()
{
    if (schedule_) {
        schedule_();
    }
}

} // namespace awm
