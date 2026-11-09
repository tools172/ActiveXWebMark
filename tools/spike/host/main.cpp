// Minimal C++ host for the Mermaid sidecar spike.
//
// Spawns: node sidecar.cjs --mermaid mermaid.min.js
// Sends:  {"id":1,"source":"...","out":"..."}
// Reads:  {"id":1,"ok":true,...}
//
// Usage:
//   awm_spike_host.exe <sidecar.cjs> <mermaid.min.js> <outDir>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string Utf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                          result.data(), size, nullptr, nullptr);
    return result;
}

std::string JsonEscape(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 16);
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 0x20) {
                char buffer[8] = {};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                result += buffer;
            } else {
                result.push_back(static_cast<char>(ch));
            }
        }
    }
    return result;
}

std::wstring FullPath(const std::wstring& path)
{
    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD length = ::GetFullPathNameW(path.c_str(), ARRAYSIZE(buffer), buffer, nullptr);
    if (length == 0 || length >= ARRAYSIZE(buffer)) {
        return path;
    }
    return buffer;
}

std::wstring FindNode()
{
    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD length = ::SearchPathW(nullptr, L"node.exe", nullptr,
                                       ARRAYSIZE(buffer), buffer, nullptr);
    if (length > 0 && length < ARRAYSIZE(buffer)) {
        return buffer;
    }
    wchar_t env[MAX_PATH * 4] = {};
    const DWORD envLength = ::GetEnvironmentVariableW(L"AWM_NODE", env, ARRAYSIZE(env));
    if (envLength > 0 && envLength < ARRAYSIZE(env)) {
        return env;
    }
    return {};
}

bool ReadLine(HANDLE pipe, std::string& line, DWORD timeoutMs)
{
    line.clear();
    const ULONGLONG deadline = ::GetTickCount64() + timeoutMs;
    for (;;) {
        DWORD available = 0;
        if (!::PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        if (available == 0) {
            if (::GetTickCount64() > deadline) {
                return false;
            }
            ::Sleep(5);
            continue;
        }

        char ch = 0;
        DWORD read = 0;
        if (!::ReadFile(pipe, &ch, 1, &read, nullptr) || read == 0) {
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            line.push_back(ch);
        }
    }
}

bool WriteAll(HANDLE pipe, const std::string& text)
{
    std::size_t offset = 0;
    while (offset < text.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(text.size() - offset, 64 * 1024));
        if (!::WriteFile(pipe, text.data() + offset, chunk, &written, nullptr) ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

struct Child {
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    HANDLE input = nullptr;
    HANDLE output = nullptr;
};

void CloseChild(Child& child)
{
    if (child.input != nullptr) {
        ::CloseHandle(child.input);
        child.input = nullptr;
    }
    if (child.process != nullptr) {
        ::WaitForSingleObject(child.process, 10000);
        ::CloseHandle(child.process);
        child.process = nullptr;
    }
    if (child.output != nullptr) {
        ::CloseHandle(child.output);
        child.output = nullptr;
    }
    if (child.job != nullptr) {
        ::CloseHandle(child.job);
        child.job = nullptr;
    }
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    ::SetConsoleOutputCP(CP_UTF8);

    if (argc < 4) {
        std::wcerr << L"usage: awm_spike_host.exe <sidecar.cjs> <mermaid.min.js> <outDir>\n";
        return 2;
    }

    const std::wstring nodePath = FindNode();
    if (nodePath.empty()) {
        std::wcerr << L"node.exe not found on PATH (set AWM_NODE to override)\n";
        return 2;
    }

    const std::wstring sidecarPath = FullPath(argv[1]);
    const std::wstring mermaidPath = FullPath(argv[2]);
    const std::wstring outDir = FullPath(argv[3]);
    if (!fs::exists(sidecarPath) || !fs::exists(mermaidPath)) {
        std::wcerr << L"sidecar or mermaid bundle not found\n";
        return 2;
    }
    std::error_code ec;
    fs::create_directories(outDir, ec);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE inputRead = nullptr;
    HANDLE inputWrite = nullptr;
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!::CreatePipe(&inputRead, &inputWrite, &sa, 0) ||
        !::CreatePipe(&outputRead, &outputWrite, &sa, 0)) {
        std::wcerr << L"CreatePipe failed\n";
        return 2;
    }
    ::SetHandleInformation(inputWrite, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);

    // Sidecar diagnostics are not part of the spike result; discard them so a
    // full stderr pipe can never block the renderer.
    HANDLE nul = ::CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &sa, OPEN_EXISTING, 0, nullptr);
    if (nul == INVALID_HANDLE_VALUE) {
        nul = nullptr;
    }

    std::wstring commandLine = L"\"" + nodePath + L"\" \"" + sidecarPath +
                               L"\" --idle-ms 60000 --mermaid \"" + mermaidPath + L"\"";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = inputRead;
    startup.hStdOutput = outputWrite;
    startup.hStdError = nul;

    PROCESS_INFORMATION process = {};
    const std::wstring workingDir = fs::path(sidecarPath).parent_path().wstring();
    const BOOL created = ::CreateProcessW(
        nodePath.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, workingDir.c_str(),
        &startup, &process);

    ::CloseHandle(inputRead);
    ::CloseHandle(outputWrite);
    if (nul != nullptr) {
        ::CloseHandle(nul);
        nul = nullptr;
    }

    if (!created) {
        std::wcerr << L"CreateProcess failed: " << ::GetLastError() << L"\n";
        ::CloseHandle(inputWrite);
        ::CloseHandle(outputRead);
        return 2;
    }

    Child child;
    child.process = process.hProcess;
    child.input = inputWrite;
    child.output = outputRead;
    ::CloseHandle(process.hThread);

    child.job = ::CreateJobObjectW(nullptr, nullptr);
    if (child.job != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(child.job, JobObjectExtendedLimitInformation,
                                  &limits, sizeof(limits));
        ::AssignProcessToJobObject(child.job, child.process);
    }

    const std::wstring flow = L"graph TD; A[开始]-->B[结束];";
    const std::wstring complex =
        L"graph TD\n  A[开始] -->|条件| B{判断}\n  B -->|是| C[执行]\n"
        L"  B -->|否| D[结束]\n  subgraph 子流程\n    C --> E[记录]\n  end";
    const std::vector<std::pair<std::wstring, std::wstring>> cases = {
        {L"flow", flow},
        {L"complex", complex},
    };

    int failures = 0;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const fs::path outPath = fs::path(outDir) / (cases[index].first + L".svg");
        const std::string request = "{\"id\":" + std::to_string(index + 1) +
                                    ",\"source\":\"" + JsonEscape(Utf8(cases[index].second)) +
                                    "\",\"out\":\"" + JsonEscape(Utf8(outPath.wstring())) +
                                    "\"}\n";

        const auto started = std::chrono::steady_clock::now();
        std::string reply;
        if (!WriteAll(child.input, request) || !ReadLine(child.output, reply, 30000)) {
            std::cerr << Utf8(cases[index].first) << ": sidecar did not reply\n";
            ++failures;
            continue;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();

        const bool ok = reply.find("\"ok\":true") != std::string::npos;
        const bool exists = fs::exists(outPath);
        std::cout << Utf8(cases[index].first) << ": " << elapsed << "ms ok="
                  << (ok ? "true" : "false") << " file=" << (exists ? "yes" : "no")
                  << " reply=" << reply << "\n";
        if (!ok || !exists) {
            ++failures;
        }
    }

    const fs::path flowSvg = fs::path(outDir) / L"flow.svg";
    if (fs::exists(flowSvg)) {
        std::ifstream input(flowSvg, std::ios::binary);
        const std::string svg((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
        const fs::path preview = fs::path(outDir) / L"spike-preview.md";
        std::ofstream output(preview, std::ios::binary);
        output << "# Mermaid sidecar (C++ host)\n\n" << svg << "\n";
        std::cout << "preview: " << Utf8(preview.wstring()) << "\n";
    }

    // Close stdin so the sidecar performs its graceful shutdown.
    ::CloseHandle(child.input);
    child.input = nullptr;
    ::WaitForSingleObject(child.process, 15000);

    CloseChild(child);
    std::cout << "failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
