#include "updateservice.h"

#include <exception>
#include <system_error>
#include <utility>

namespace awm {

UpdateService::~UpdateService()
{
    Shutdown();
}

bool UpdateService::StartCheck(const std::string& currentVersion)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Idle) {
            return false;
        }
        state_ = State::Checking;
        cancel_.store(false);
        downloaded_.store(0);
        total_.store(0);
        checkResult_.reset();
        downloadResult_.reset();
    }

    // The previous worker has already published Idle, so joining is immediate.
    joinWorker();

    try {
        worker_ = std::thread([this, currentVersion]() {
            CheckResult result;
            try {
                result = FetchLatestRelease(currentVersion, cancel_);
            } catch (const std::exception& error) {
                result.status = CheckStatus::NetworkError;
                result.error = std::string("检查更新时发生异常：") + error.what();
            } catch (...) {
                result.status = CheckStatus::NetworkError;
                result.error = "检查更新时发生未知异常。";
            }
            std::lock_guard<std::mutex> lock(mutex_);
            checkResult_ = std::move(result);
            state_ = State::Idle;
        });
    } catch (const std::system_error&) {
        std::lock_guard<std::mutex> lock(mutex_);
        CheckResult result;
        result.status = CheckStatus::NetworkError;
        result.error = "无法启动更新检查线程。";
        checkResult_ = std::move(result);
        state_ = State::Idle;
    }
    return true;
}

bool UpdateService::StartDownload(const ReleaseAsset& asset, const std::wstring& targetPath)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Idle) {
            return false;
        }
        state_ = State::Downloading;
        cancel_.store(false);
        downloaded_.store(0);
        total_.store(asset.size);
        checkResult_.reset();
        downloadResult_.reset();
    }

    joinWorker();

    try {
        worker_ = std::thread([this, asset, targetPath]() {
            DownloadResult result;
            try {
                result = DownloadInstaller(asset, targetPath, downloaded_, total_, cancel_);
            } catch (const std::exception& error) {
                result.error = std::string("下载更新时发生异常：") + error.what();
            } catch (...) {
                result.error = "下载更新时发生未知异常。";
            }
            std::lock_guard<std::mutex> lock(mutex_);
            downloadResult_ = std::move(result);
            state_ = State::Idle;
        });
    } catch (const std::system_error&) {
        std::lock_guard<std::mutex> lock(mutex_);
        DownloadResult result;
        result.error = "无法启动更新下载线程。";
        downloadResult_ = std::move(result);
        state_ = State::Idle;
    }
    return true;
}

bool UpdateService::IsBusy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ != State::Idle;
}

UpdateService::State UpdateService::state() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::optional<CheckResult> UpdateService::TakeCheckResult()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::optional<CheckResult> result = std::move(checkResult_);
    checkResult_.reset();
    return result;
}

std::optional<DownloadResult> UpdateService::TakeDownloadResult()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::optional<DownloadResult> result = std::move(downloadResult_);
    downloadResult_.reset();
    return result;
}

UpdateService::DownloadSnapshot UpdateService::DownloadProgress() const
{
    DownloadSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.active = state_ == State::Downloading;
    }
    snapshot.downloaded = downloaded_.load();
    snapshot.total = total_.load();
    return snapshot;
}

void UpdateService::RequestCancel()
{
    cancel_.store(true);
}

void UpdateService::Shutdown()
{
    cancel_.store(true);
    joinWorker();
}

void UpdateService::joinWorker()
{
    if (worker_.joinable()) {
        worker_.join();
    }
}

} // namespace awm
