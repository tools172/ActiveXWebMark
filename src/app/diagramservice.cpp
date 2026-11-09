#include "diagramservice.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "apppaths.h"
#include "util/stringutil.h"

namespace awm {

namespace {

std::filesystem::path ExecutableDirectory()
{
    wchar_t path[32768] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (length == 0 || length >= ARRAYSIZE(path)) {
        return {};
    }
    return std::filesystem::path(path).parent_path();
}

std::wstring EnvValue(const wchar_t* name)
{
    wchar_t buffer[4096] = {};
    const DWORD length = ::GetEnvironmentVariableW(name, buffer, ARRAYSIZE(buffer));
    return (length > 0 && length < ARRAYSIZE(buffer))
               ? std::wstring(buffer, length)
               : std::wstring();
}

std::wstring FindNode()
{
    // Tests and portable setups can force a path. An invalid value must not
    // silently fall back to PATH; otherwise a "missing Node" test would pass
    // by finding the developer's installed Node.
    wchar_t overridePath[32768] = {};
    const DWORD overrideLength =
        ::GetEnvironmentVariableW(L"AWM_NODE", overridePath, ARRAYSIZE(overridePath));
    if (overrideLength > 0 && overrideLength < ARRAYSIZE(overridePath)) {
        std::error_code ec;
        return std::filesystem::is_regular_file(overridePath, ec) && !ec
                   ? std::wstring(overridePath)
                   : std::wstring();
    }

    wchar_t path[32768] = {};
    const DWORD length = ::SearchPathW(nullptr, L"node.exe", nullptr,
                                       ARRAYSIZE(path), path, nullptr);
    if (length > 0 && length < ARRAYSIZE(path)) {
        return path;
    }

    const wchar_t* candidates[] = {
        L"%ProgramFiles%\\nodejs\\node.exe",
        L"%ProgramFiles(x86)%\\nodejs\\node.exe",
        L"%LOCALAPPDATA%\\Programs\\nodejs\\node.exe",
    };
    for (const wchar_t* candidate : candidates) {
        wchar_t expanded[32768] = {};
        const DWORD expandedLength =
            ::ExpandEnvironmentStringsW(candidate, expanded, ARRAYSIZE(expanded));
        if (expandedLength == 0 || expandedLength > ARRAYSIZE(expanded)) {
            continue;
        }
        std::error_code ec;
        if (std::filesystem::is_regular_file(expanded, ec) && !ec) {
            return expanded;
        }
    }
    return {};
}

std::wstring FindPlantUmlExe()
{
    const std::filesystem::path exeDir = ExecutableDirectory();
    if (!exeDir.empty()) {
        const std::filesystem::path candidates[] = {
            exeDir / L"plantuml" / L"plantuml-headless.exe",
            exeDir / L"plantuml-headless.exe",
            exeDir / L"plantuml" / L"plantuml.exe",
            exeDir / L"plantuml.exe",
            exeDir / L"mermaid" / L"plantuml-headless.exe",
        };
        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
                return candidate.wstring();
            }
        }
    }

    const wchar_t* candidates[] = {
        L"%ProgramFiles%\\PlantUML\\plantuml-headless.exe",
        L"%ProgramFiles%\\PlantUML\\plantuml.exe",
    };
    for (const wchar_t* candidate : candidates) {
        wchar_t expanded[32768] = {};
        const DWORD expandedLength =
            ::ExpandEnvironmentStringsW(candidate, expanded, ARRAYSIZE(expanded));
        if (expandedLength == 0 || expandedLength > ARRAYSIZE(expanded)) {
            continue;
        }
        std::error_code ec;
        if (std::filesystem::is_regular_file(expanded, ec) && !ec) {
            return expanded;
        }
    }
    return {};
}

bool ReadNodeMajor(const std::wstring& nodePath, int& major)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!::CreatePipe(&outputRead, &outputWrite, &sa, 0)) {
        return false;
    }
    ::SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE nulInput = ::CreateFileW(L"NUL", GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                    OPEN_EXISTING, 0, nullptr);
    if (nulInput == INVALID_HANDLE_VALUE) {
        nulInput = nullptr;
    }

    std::wstring commandLine = L"\"" + nodePath + L"\" --version";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nulInput;
    startup.hStdOutput = outputWrite;
    startup.hStdError = outputWrite;

    PROCESS_INFORMATION process = {};
    const BOOL created = ::CreateProcessW(
        nodePath.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startup,
        &process);

    ::CloseHandle(outputWrite);
    if (nulInput != nullptr) {
        ::CloseHandle(nulInput);
    }
    if (!created) {
        ::CloseHandle(outputRead);
        return false;
    }
    ::CloseHandle(process.hThread);

    std::string output;
    char buffer[128];
    for (;;) {
        DWORD read = 0;
        if (!::ReadFile(outputRead, buffer, sizeof(buffer), &read, nullptr) || read == 0) {
            break;
        }
        output.append(buffer, read);
        if (output.size() > 256) {
            break;
        }
    }
    if (::WaitForSingleObject(process.hProcess, 5000) == WAIT_TIMEOUT) {
        ::TerminateProcess(process.hProcess, 1);
        ::WaitForSingleObject(process.hProcess, 1000);
    }
    DWORD exitCode = 1;
    ::GetExitCodeProcess(process.hProcess, &exitCode);
    ::CloseHandle(process.hProcess);
    ::CloseHandle(outputRead);
    if (exitCode != 0) {
        return false;
    }

    std::size_t pos = 0;
    while (pos < output.size() &&
           !std::isdigit(static_cast<unsigned char>(output[pos]))) {
        ++pos;
    }
    if (pos >= output.size()) {
        return false;
    }
    int value = 0;
    while (pos < output.size() &&
           std::isdigit(static_cast<unsigned char>(output[pos]))) {
        value = value * 10 + (output[pos] - '0');
        ++pos;
        if (value > 1000) {
            break;
        }
    }
    major = value;
    return true;
}

} // namespace

DiagramService::DiagramService()
{
    if (EnvValue(L"AWM_MERMAID_DISABLE") == L"1") {
        mermaidDisabled_ = true;
    }
    if (EnvValue(L"AWM_PLANTUML_DISABLE") == L"1") {
        plantUmlDisabled_ = true;
    }
    if (EnvValue(L"AWM_DIAGRAM_DISABLE") == L"1") {
        mermaidDisabled_ = true;
        plantUmlDisabled_ = true;
    }
    const std::wstring server = EnvValue(L"AWM_PLANTUML_SERVER");
    if (!server.empty()) {
        plantUmlServer_ = wideToUtf8(server);
        plantUmlServerFromEnv_ = true;
    }
    const std::wstring mode = EnvValue(L"AWM_PLANTUML_MODE");
    if (!mode.empty()) {
        const std::string utf8 = wideToUtf8(mode);
        if (utf8 == "local" || utf8 == "remote" || utf8 == "auto") {
            plantUmlMode_ = utf8;
            plantUmlModeFromEnv_ = true;
        }
    }
    const std::wstring exe = EnvValue(L"AWM_PLANTUML_EXE");
    if (!exe.empty()) {
        plantUmlExe_ = exe;
        plantUmlExeFromEnv_ = true;
    }
}

DiagramService::~DiagramService()
{
    Shutdown();
}

bool DiagramService::ResolveNativePlantUml()
{
    // An explicitly configured path is trusted: if it is wrong, do not silently
    // pick a different executable. Empty settings fall back to auto-detection.
    if (!plantUmlExe_.empty()) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(plantUmlExe_, ec) || ec) {
            plantUmlExe_.clear();
        }
    } else {
        plantUmlExe_ = FindPlantUmlExe();
    }

    plantUmlExeSignature_.clear();
    if (!plantUmlExe_.empty()) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(plantUmlExe_, ec);
        if (!ec) {
            const auto modified = std::filesystem::last_write_time(plantUmlExe_, ec);
            if (!ec) {
                plantUmlExeSignature_ =
                    std::to_string(size) + ":" +
                    std::to_string(modified.time_since_epoch().count());
            }
        }
    }

    return !plantUmlExe_.empty();
}

bool DiagramService::Detect()
{
    if (detected_) {
        return available_;
    }
    detected_ = true;

    nodePath_ = FindNode();
    if (nodePath_.empty()) {
        return false;
    }

    int major = 0;
    if (!ReadNodeMajor(nodePath_, major) || major < kMinimumNodeMajor) {
        return false;
    }

    const std::filesystem::path exeDir = ExecutableDirectory();
    if (exeDir.empty()) {
        return false;
    }
    sidecarPath_ = (exeDir / L"mermaid" / L"sidecar.cjs").wstring();
    mermaidPath_ = (exeDir / L"mermaid" / L"mermaid.min.js").wstring();

    std::error_code ec;
    if (!std::filesystem::is_regular_file(sidecarPath_, ec) || ec) {
        return false;
    }
    mermaidBundlePresent_ =
        std::filesystem::is_regular_file(mermaidPath_, ec) && !ec;

    cacheDirectory_ = diagramCacheDirectory();
    std::filesystem::create_directories(cacheDirectory_, ec);
    if (ec) {
        return false;
    }

    ResolveNativePlantUml();
    available_ = true;
    return true;
}

bool DiagramService::Available()
{
    return Detect();
}

bool DiagramService::MermaidEnabled() const
{
    return available_.load() && mermaidBundlePresent_ && !mermaidDisabled_;
}

bool DiagramService::PlantUmlEnabled() const
{
    if (!available_.load() || !plantUmlEnabled_ || plantUmlDisabled_) {
        return false;
    }
    // Local-only mode without a usable native executable falls back to the
    // original source text instead of silently using a remote server.
    if (plantUmlMode_ == "local" && plantUmlExe_.empty()) {
        return false;
    }
    return true;
}

std::string DiagramService::PlantUmlBackend() const
{
    if (plantUmlMode_ == "remote") {
        return "remote";
    }
    return plantUmlExe_.empty() ? "remote" : "native";
}

std::string DiagramService::PlantUmlExe() const
{
    return wideToUtf8(plantUmlExe_);
}

std::string DiagramService::PlantUmlVersion() const
{
    if (PlantUmlBackend() == "native") {
        return std::string(kPlantUmlVersion) + ":native:" + plantUmlExeSignature_;
    }
    return kPlantUmlVersion;
}

void DiagramService::SetPlantUmlMode(std::string mode)
{
    if (plantUmlModeFromEnv_) {
        return;
    }
    if (mode == "local" || mode == "remote" || mode == "auto") {
        plantUmlMode_ = std::move(mode);
    }
}

void DiagramService::SetPlantUmlExe(std::string exe)
{
    if (plantUmlExeFromEnv_) {
        return;
    }
    plantUmlExe_ = utf8ToWide(exe);
}

void DiagramService::SetPlantUmlServer(std::string server)
{
    if (!plantUmlServerFromEnv_ && !server.empty()) {
        plantUmlServer_ = std::move(server);
    }
}

bool DiagramService::Start()
{
    if (running_) {
        return true;
    }
    if (!Available()) {
        return false;
    }
    Stop();

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE inputRead = nullptr;
    HANDLE inputWrite = nullptr;
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    // The sidecar starts Edge before reading stdin. A 1 MiB request buffer
    // lets a large diagram be queued without blocking the UI thread while the
    // browser starts. Replies are small.
    if (!::CreatePipe(&inputRead, &inputWrite, &sa, 1024 * 1024) ||
        !::CreatePipe(&outputRead, &outputWrite, &sa, 64 * 1024)) {
        if (inputRead) {
            ::CloseHandle(inputRead);
        }
        if (inputWrite) {
            ::CloseHandle(inputWrite);
        }
        if (outputRead) {
            ::CloseHandle(outputRead);
        }
        if (outputWrite) {
            ::CloseHandle(outputWrite);
        }
        available_ = false;
        return false;
    }
    ::SetHandleInformation(inputWrite, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul = ::CreateFileW(L"NUL", GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                               OPEN_EXISTING, 0, nullptr);
    if (nul == INVALID_HANDLE_VALUE) {
        nul = nullptr;
    }

    std::wstring commandLine = L"\"" + nodePath_ + L"\" \"" + sidecarPath_ +
                               L"\" --idle-ms " + std::to_wstring(kIdleTimeoutMs) +
                               L" --mermaid \"" + mermaidPath_ + L"\"";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = inputRead;
    startup.hStdOutput = outputWrite;
    startup.hStdError = nul != nullptr ? nul : ::GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION process = {};
    const BOOL created = ::CreateProcessW(
        nodePath_.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startup,
        &process);

    ::CloseHandle(inputRead);
    ::CloseHandle(outputWrite);
    if (nul != nullptr) {
        ::CloseHandle(nul);
    }
    if (!created) {
        ::CloseHandle(inputWrite);
        ::CloseHandle(outputRead);
        available_ = false;
        return false;
    }
    ::CloseHandle(process.hThread);

    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                                  sizeof(limits));
        ::AssignProcessToJobObject(job, process.hProcess);
    }

    process_ = process.hProcess;
    job_ = job;
    input_ = inputWrite;
    output_ = outputRead;
    running_ = true;
    reader_ = std::thread(&DiagramService::ReaderLoop, this);
    return true;
}

void DiagramService::Stop()
{
    if (input_ != nullptr) {
        ::CloseHandle(input_);
        input_ = nullptr;
    }
    if (process_ != nullptr) {
        if (::WaitForSingleObject(process_, 5000) == WAIT_TIMEOUT) {
            ::TerminateProcess(process_, 1);
            ::WaitForSingleObject(process_, 1000);
        }
    }
    if (job_ != nullptr) {
        ::CloseHandle(job_);
        job_ = nullptr;
    }
    if (reader_.joinable()) {
        reader_.join();
    }
    if (output_ != nullptr) {
        ::CloseHandle(output_);
        output_ = nullptr;
    }
    if (process_ != nullptr) {
        ::CloseHandle(process_);
        process_ = nullptr;
    }
    running_ = false;
}

void DiagramService::ReaderLoop()
{
    std::string buffer;
    char chunk[4096];
    for (;;) {
        DWORD read = 0;
        if (!::ReadFile(output_, chunk, sizeof(chunk), &read, nullptr) || read == 0) {
            break;
        }
        buffer.append(chunk, read);
        for (;;) {
            const std::size_t newline = buffer.find('\n');
            if (newline == std::string::npos) {
                break;
            }
            std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (line.empty()) {
                continue;
            }
            try {
                const nlohmann::json message = nlohmann::json::parse(line);
                const int id = message.value("id", 0);
                if (id == 0) {
                    continue;
                }
                const bool ok = message.value("ok", false);
                Complete(id, ok ? std::string()
                                : message.value("error", std::string("render failed")));
            } catch (const nlohmann::json::exception&) {
                // Ignore malformed diagnostics; stdout is JSON-only.
            }
        }
    }

    DWORD exitCode = 0;
    if (process_ != nullptr) {
        ::GetExitCodeProcess(process_, &exitCode);
    }
    running_ = false;
    if (exitCode != 0) {
        available_ = false;
    }
    FailAllPending(exitCode == 0 ? "renderer stopped"
                                 : "renderer failed to start");
}

bool DiagramService::WriteLine(const std::string& line)
{
    if (input_ == nullptr) {
        return false;
    }
    std::size_t offset = 0;
    while (offset < line.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(line.size() - offset, 64 * 1024));
        if (!::WriteFile(input_, line.data() + offset, chunk, &written, nullptr) ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

void DiagramService::Complete(int id, const std::string& error)
{
    Pending pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = pending_.find(id);
        if (it == pending_.end()) {
            return;
        }
        pending = std::move(it->second);
        pending_.erase(it);
    }

    std::string failure = error;
    if (failure.empty()) {
        std::error_code ec;
        const bool exists =
            std::filesystem::is_regular_file(pending.block.cachePath, ec) && !ec;
        if (!exists) {
            failure = "renderer produced no SVG";
        }
    }

    for (const auto& entry : pending.callbacks) {
        if (!entry.callback) {
            continue;
        }
        DiagramRenderResult result;
        result.elementId = entry.elementId;
        result.ok = failure.empty();
        result.url = pending.block.url;
        result.error = failure;
        try {
            entry.callback(result);
        } catch (...) {
            // A failing UI callback must not kill the reader thread.
        }
    }
}

void DiagramService::FailAllPending(const std::string& error)
{
    std::map<int, Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(pending_);
    }
    for (const auto& entry : pending) {
        for (const auto& callbackEntry : entry.second.callbacks) {
            if (!callbackEntry.callback) {
                continue;
            }
            DiagramRenderResult result;
            result.elementId = callbackEntry.elementId;
            result.ok = false;
            result.error = error;
            try {
                callbackEntry.callback(result);
            } catch (...) {
                // Ignore.
            }
        }
    }
}

bool DiagramService::Request(const DiagramPendingBlock& block,
                             std::function<void(DiagramRenderResult)> callback)
{
    if (!Available()) {
        DiagramRenderResult result;
        result.elementId = block.elementId;
        result.error = "Mermaid renderer is unavailable";
        if (callback) {
            callback(result);
        }
        return false;
    }
    if (!Start()) {
        DiagramRenderResult result;
        result.elementId = block.elementId;
        result.error = "Mermaid renderer failed to start";
        if (callback) {
            callback(result);
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : pending_) {
            if (entry.second.block.cachePath == block.cachePath) {
                entry.second.callbacks.push_back({block.elementId, std::move(callback)});
                return true;
            }
        }
    }

    const int id = nextId_++;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Pending pending;
        pending.block = block;
        pending.callbacks.push_back({block.elementId, std::move(callback)});
        pending_[id] = std::move(pending);
    }

    try {
        nlohmann::json request;
        request["id"] = id;
        request["kind"] = DiagramKindName(block.kind);
        request["source"] = block.source;
        request["out"] = wideToUtf8(block.cachePath.wstring());
        if (block.kind == DiagramKind::Mermaid) {
            request["theme"] = block.theme;
        } else {
            const std::string mode = block.mode.empty() ? "remote" : block.mode;
            request["mode"] = mode;
            request["dark"] = block.dark;
            if (mode == "native") {
                request["exe"] = block.exe;
            } else {
                request["server"] = block.server;
            }
        }
        if (!WriteLine(request.dump() + "\n")) {
            Complete(id, "renderer is not running");
            return false;
        }
    } catch (const std::exception&) {
        // nlohmann::json rejects invalid UTF-8. Treat it as a render failure
        // instead of letting the exception escape onto the UI thread.
        Complete(id, "diagram text is not valid UTF-8");
        return false;
    }
    return true;
}

void DiagramService::Shutdown()
{
    Stop();
    FailAllPending("renderer shut down");
}

} // namespace awm
