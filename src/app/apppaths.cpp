#include "apppaths.h"

#include <Windows.h>
#include <ShlObj.h>

#include <system_error>

#include "settings/jsonfilesettings.h"

namespace awm {

namespace {

std::filesystem::path executableDirectory()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        return ec ? std::filesystem::path() : cwd;
    }
    return std::filesystem::path(buffer, buffer + length).parent_path();
}

} // namespace

std::filesystem::path settingsFilePath()
{
    // Test/portable override used by tests/manual/tabstrip-smoke.ps1. It keeps
    // UI smoke tests from touching the user's real settings file.
    wchar_t overrideDir[1024] = {};
    const DWORD overrideLength =
        ::GetEnvironmentVariableW(L"AWM_SETTINGS_DIR", overrideDir, 1024);
    if (overrideLength > 0 && overrideLength < 1024) {
        return std::filesystem::path(overrideDir) / L"settings.json";
    }

    PWSTR localAppData = nullptr;
    if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                         &localAppData)) &&
        localAppData != nullptr) {
        std::filesystem::path directory(localAppData);
        ::CoTaskMemFree(localAppData);
        directory /= L"ActiveXWebMark";
        return directory / L"settings.json";
    }

    return executableDirectory() / L"ActiveXWebMark.settings.json";
}

std::unique_ptr<ISettings> createSettings()
{
    const std::filesystem::path path = settingsFilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    return std::make_unique<JsonFileSettings>(path);
}

std::filesystem::path updateDirectory()
{
    return settingsFilePath().parent_path() / L"updates";
}

std::filesystem::path themesDirectory()
{
    return settingsFilePath().parent_path() / L"themes";
}

std::filesystem::path diagramCacheDirectory()
{
    return settingsFilePath().parent_path() / L"diagram-cache";
}

} // namespace awm
