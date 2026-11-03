#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "update/updateclient.h"

namespace awm {

/// Runs update checks and downloads on a worker thread and exposes their
/// results to the UI thread. The UI polls this object from a timer; it does not
/// post messages back, so a closing window cannot leave a dangling callback.
class UpdateService {
public:
    enum class State {
        Idle,
        Checking,
        Downloading,
    };

    struct DownloadSnapshot {
        bool active = false;
        long long downloaded = 0;
        long long total = 0;
    };

    UpdateService() = default;
    ~UpdateService();

    UpdateService(const UpdateService&) = delete;
    UpdateService& operator=(const UpdateService&) = delete;

    /// Starts a check when idle. Returns false when another operation is active.
    bool StartCheck(const std::string& currentVersion);

    /// Starts downloading `asset` to `targetPath` when idle.
    bool StartDownload(const ReleaseAsset& asset, const std::wstring& targetPath);

    bool IsBusy() const;
    State state() const;

    /// Takes the completed check result, if one is waiting.
    std::optional<CheckResult> TakeCheckResult();
    /// Takes the completed download result, if one is waiting.
    std::optional<DownloadResult> TakeDownloadResult();

    DownloadSnapshot DownloadProgress() const;

    /// Requests cancellation of the active operation. The worker exits at its
    /// next cancellation point and still publishes a result.
    void RequestCancel();

    /// Cancels and waits for the worker. Called by the destructor.
    void Shutdown();

private:
    void joinWorker();

    mutable std::mutex mutex_;
    State state_ = State::Idle;
    std::optional<CheckResult> checkResult_;
    std::optional<DownloadResult> downloadResult_;
    std::atomic_bool cancel_{false};
    std::atomic<long long> downloaded_{0};
    std::atomic<long long> total_{0};
    std::thread worker_;
};

} // namespace awm
