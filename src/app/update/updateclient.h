#pragma once

#include <atomic>
#include <string>

#include "update/updateinfo.h"

namespace awm {

enum class CheckStatus {
    Ok,
    NoRelease,
    RateLimited,
    NetworkError,
    ParseError,
    Cancelled,
};

struct CheckResult {
    CheckStatus status = CheckStatus::NetworkError;
    bool hasUpdate = false;
    ReleaseInfo release;
    std::string error;
};

struct DownloadResult {
    bool success = false;
    bool cancelled = false;
    std::wstring path;
    std::string error;
};

/// Queries the GitHub "latest release" API and compares it with
/// `currentVersion`. Blocking; call from a worker thread.
CheckResult FetchLatestRelease(const std::string& currentVersion,
                               const std::atomic_bool& cancel);

/// Downloads `asset` to `targetPath` over HTTPS, reporting progress through the
/// two variables. The download is written to a sibling ".part" file and moved
/// into place only after size and (when provided) SHA-256 verification.
DownloadResult DownloadInstaller(const ReleaseAsset& asset, const std::wstring& targetPath,
                                 std::atomic<long long>& downloaded,
                                 std::atomic<long long>& total,
                                 const std::atomic_bool& cancel);

/// Starts the downloaded installer after a short delay so this process can
/// exit and the installer can replace its executable. MSI packages are run
/// through msiexec.
bool LaunchInstaller(const std::wstring& installerPath);

} // namespace awm
