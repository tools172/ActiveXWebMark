#pragma once

#include <cstdint>
#include <functional>

namespace awm {

class IEditorView;
class IPreviewView;

/// Coordinates scroll synchronization between the source editor and preview.
///
/// The editor and preview adapters only expose capability: report the first
/// visible source line and scroll to a source line. This controller owns the
/// policy: debouncing, echo suppression and which pane is currently driving.
/// It has no window or timer of its own; the host injects a scheduler.
class ScrollSyncController {
public:
    using Clock = std::function<std::uint64_t()>;
    using Schedule = std::function<void()>;
    using CancelSchedule = std::function<void()>;

    explicit ScrollSyncController(IEditorView& editor, Clock clock = {});
    ~ScrollSyncController() = default;

    ScrollSyncController(const ScrollSyncController&) = delete;
    ScrollSyncController& operator=(const ScrollSyncController&) = delete;

    /// Sets (or clears) the preview pane. The preview is created lazily by the
    /// host, so this may be called after construction.
    void SetPreview(IPreviewView* preview);

    /// Injects the host timer hooks. `schedule` is called when a short delay
    /// should elapse before OnTimerTick(); `cancel` cancels that timer.
    void SetScheduler(Schedule schedule, CancelSchedule cancel);

    void SetEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    /// Events from the two panes.
    void OnEditorScrolled(int firstVisibleLine);
    void OnPreviewScrolled(double sourceLine);

    /// Called by the host when the scheduled delay has elapsed.
    void OnTimerTick();

    /// Aligns the preview to the editor's current first visible line.
    void AlignPreviewToEditor();

    /// Drops any queued synchronization and cancels the host timer.
    void Cancel();

    /// Ignores incoming scroll events from both panes for `durationMs`.
    /// Used while the document is re-rendered or the TOC moves both panes.
    void Suspend(std::uint64_t durationMs);

private:
    enum class Source {
        None,
        Editor,
        Preview,
    };

    std::uint64_t Now() const;
    void ScheduleApply();

    IEditorView& editor_;
    IPreviewView* preview_ = nullptr;
    Clock clock_;
    Schedule schedule_;
    CancelSchedule cancelSchedule_;

    bool enabled_ = true;
    Source pendingSource_ = Source::None;
    int pendingPreviewLine_ = 0;
    int pendingEditorLine_ = 0;
    std::uint64_t suppressEditorUntil_ = 0;
    std::uint64_t suppressPreviewUntil_ = 0;
};

} // namespace awm
