#include "updateclient.h"

#include <Windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "util/stringutil.h"
#include "version.h"

namespace awm {

namespace {

constexpr const wchar_t* kLatestReleaseUrl =
    L"https://api.github.com/repos/tools172/ActiveXWebMark/releases/latest";
constexpr DWORD kMaxApiResponseBytes = 2 * 1024 * 1024;
constexpr long long kMaxDownloadBytes = 512LL * 1024 * 1024;
constexpr DWORD kDownloadChunkBytes = 64 * 1024;

class WinHttpHandle {
public:
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : handle_(handle) {}
    ~WinHttpHandle() { reset(); }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept
    {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    void reset(HINTERNET handle = nullptr)
    {
        if (handle_ != nullptr) {
            ::WinHttpCloseHandle(handle_);
        }
        handle_ = handle;
    }

    HINTERNET get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_ = nullptr;
};

struct ParsedUrl {
    bool valid = false;
    bool secure = false;
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
};

bool parseUrl(const std::wstring& url, ParsedUrl& parsed)
{
    if (url.empty()) {
        return false;
    }

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!::WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        return false;
    }

    parsed.valid = true;
    parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.port = components.nPort;
    parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0 && components.lpszExtraInfo != nullptr) {
        parsed.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (parsed.path.empty()) {
        parsed.path = L"/";
    }
    return true;
}

bool endsWithNoCase(const std::wstring& text, const std::wstring& suffix)
{
    return text.size() >= suffix.size() &&
           ::_wcsicmp(text.c_str() + text.size() - suffix.size(), suffix.c_str()) == 0;
}

bool isTrustedDownloadHost(const std::wstring& host)
{
    if (::_wcsicmp(host.c_str(), L"github.com") == 0 ||
        ::_wcsicmp(host.c_str(), L"api.github.com") == 0 ||
        ::_wcsicmp(host.c_str(), L"githubusercontent.com") == 0) {
        return true;
    }
    return endsWithNoCase(host, L".githubusercontent.com");
}

std::string describeWinHttpError(const char* action, DWORD code)
{
    std::string message = action;
    message += "（错误码 " + std::to_string(code) + "）";
    if (code == ERROR_WINHTTP_SECURE_FAILURE) {
        message += "。无法建立安全连接，请检查系统证书或 TLS 设置。";
    } else {
        message += "。请检查网络连接或代理设置。";
    }
    return message;
}

WinHttpHandle openSession()
{
    const std::wstring agent = L"ActiveXWebMark/" + utf8ToWide(version());

    WinHttpHandle session(::WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        session.reset(::WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    }
    if (!session) {
        return {};
    }

    ::WinHttpSetTimeouts(session.get(), 5000, 5000, 10000, 15000);

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    ::WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols,
                       sizeof(protocols));
    return session;
}

/// Keeps the WinHTTP handle hierarchy (session, connection, request) alive for
/// as long as the request is in use.
struct HttpRequest {
    WinHttpHandle session;
    WinHttpHandle connection;
    WinHttpHandle request;

    explicit operator bool() const { return request.get() != nullptr; }
};

HttpRequest openRequest(const ParsedUrl& parsed, const std::wstring& headers,
                        std::string& error)
{
    HttpRequest result;
    result.session = openSession();
    if (!result.session) {
        error = describeWinHttpError("无法初始化网络连接", ::GetLastError());
        return result;
    }

    result.connection =
        WinHttpHandle(::WinHttpConnect(result.session.get(), parsed.host.c_str(), parsed.port, 0));
    if (!result.connection) {
        error = describeWinHttpError("无法连接更新服务器", ::GetLastError());
        return result;
    }

    result.request = WinHttpHandle(::WinHttpOpenRequest(
        result.connection.get(), L"GET", parsed.path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, parsed.secure ? WINHTTP_FLAG_SECURE : 0));
    if (!result.request) {
        error = describeWinHttpError("无法创建网络请求", ::GetLastError());
        return result;
    }

    if (!headers.empty() &&
        !::WinHttpAddRequestHeaders(
            result.request.get(), headers.c_str(), static_cast<DWORD>(headers.size()),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        error = describeWinHttpError("无法设置请求头", ::GetLastError());
        result.request.reset();
    }

    return result;
}

bool queryStatusCode(HINTERNET request, DWORD& status, std::string& error)
{
    status = 0;
    DWORD size = sizeof(status);
    if (!::WinHttpQueryHeaders(request,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                               WINHTTP_NO_HEADER_INDEX)) {
        error = describeWinHttpError("无法读取服务器响应", ::GetLastError());
        return false;
    }
    return true;
}

bool queryContentLength(HINTERNET request, unsigned long long& length)
{
    length = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (!::WinHttpQueryHeaders(request,
                               WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &value, &size,
                               WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    length = value;
    return true;
}

enum class HttpOutcome {
    Ok,
    Cancelled,
    Failed,
    TooLarge,
};

HttpOutcome httpGet(const std::wstring& url, const std::wstring& headers,
                    std::size_t maxBytes, const std::atomic_bool& cancel,
                    std::string& body, DWORD& statusCode, std::string& error)
{
    body.clear();
    statusCode = 0;

    ParsedUrl parsed;
    if (!parseUrl(url, parsed) || !parsed.secure) {
        error = "更新地址无效（仅支持 HTTPS）。";
        return HttpOutcome::Failed;
    }
    if (cancel.load()) {
        return HttpOutcome::Cancelled;
    }

    HttpRequest request = openRequest(parsed, headers, error);
    if (!request) {
        return HttpOutcome::Failed;
    }

    if (!::WinHttpSendRequest(request.request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = describeWinHttpError("无法发送网络请求", ::GetLastError());
        return HttpOutcome::Failed;
    }
    if (!::WinHttpReceiveResponse(request.request.get(), nullptr)) {
        error = describeWinHttpError("无法接收服务器响应", ::GetLastError());
        return HttpOutcome::Failed;
    }
    if (!queryStatusCode(request.request.get(), statusCode, error)) {
        return HttpOutcome::Failed;
    }

    for (;;) {
        if (cancel.load()) {
            return HttpOutcome::Cancelled;
        }

        DWORD available = 0;
        if (!::WinHttpQueryDataAvailable(request.request.get(), &available)) {
            error = describeWinHttpError("读取服务器响应失败", ::GetLastError());
            return HttpOutcome::Failed;
        }
        if (available == 0) {
            break;
        }
        if (body.size() + available > maxBytes) {
            error = "服务器响应内容过大。";
            return HttpOutcome::TooLarge;
        }

        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!::WinHttpReadData(request.request.get(), chunk.data(), available, &read)) {
            error = describeWinHttpError("读取服务器响应失败", ::GetLastError());
            return HttpOutcome::Failed;
        }
        if (read == 0) {
            break;
        }
        body.append(chunk.data(), read);
    }

    return HttpOutcome::Ok;
}

std::string asciiLower(const std::string& text)
{
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower;
}

enum class DigestKind {
    None,
    Sha256,
    Unsupported,
    Malformed,
};

DigestKind parseDigest(const std::string& digest, std::string& expectedHex)
{
    expectedHex.clear();
    if (digest.empty()) {
        return DigestKind::None;
    }

    const std::string lower = asciiLower(digest);
    const std::string prefix = "sha256:";
    if (lower.compare(0, prefix.size(), prefix) != 0) {
        return DigestKind::Unsupported;
    }

    const std::string hex = lower.substr(prefix.size());
    const bool valid =
        hex.size() == 64 && std::all_of(hex.begin(), hex.end(), [](unsigned char ch) {
            return std::isxdigit(ch) != 0;
        });
    if (!valid) {
        return DigestKind::Malformed;
    }
    expectedHex = hex;
    return DigestKind::Sha256;
}

class Sha256Hasher {
public:
    ~Sha256Hasher() { close(); }

    bool begin()
    {
        if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM,
                                                          nullptr, 0))) {
            return false;
        }

        DWORD objectSize = 0;
        DWORD hashSize = 0;
        DWORD written = 0;
        if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                algorithm_, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                sizeof(objectSize), &written, 0)) ||
            !BCRYPT_SUCCESS(::BCryptGetProperty(
                algorithm_, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize),
                sizeof(hashSize), &written, 0))) {
            return false;
        }

        object_.resize(objectSize);
        value_.resize(hashSize);
        return BCRYPT_SUCCESS(::BCryptCreateHash(algorithm_, &hash_, object_.data(),
                                                 objectSize, nullptr, 0, 0));
    }

    bool update(const char* data, std::size_t size)
    {
        if (size == 0) {
            return true;
        }
        return BCRYPT_SUCCESS(::BCryptHashData(hash_, reinterpret_cast<PUCHAR>(
                                                         const_cast<char*>(data)),
                                               static_cast<ULONG>(size), 0));
    }

    std::string finishHex()
    {
        if (hash_ == nullptr ||
            !BCRYPT_SUCCESS(::BCryptFinishHash(hash_, value_.data(),
                                               static_cast<ULONG>(value_.size()), 0))) {
            return {};
        }

        static constexpr char kDigits[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(value_.size() * 2);
        for (const UCHAR byte : value_) {
            hex.push_back(kDigits[byte >> 4]);
            hex.push_back(kDigits[byte & 0x0F]);
        }
        return hex;
    }

private:
    void close()
    {
        if (hash_ != nullptr) {
            ::BCryptDestroyHash(hash_);
            hash_ = nullptr;
        }
        if (algorithm_ != nullptr) {
            ::BCryptCloseAlgorithmProvider(algorithm_, 0);
            algorithm_ = nullptr;
        }
    }

    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    std::vector<UCHAR> object_;
    std::vector<UCHAR> value_;
};

void deleteQuietly(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace

CheckResult FetchLatestRelease(const std::string& currentVersion,
                               const std::atomic_bool& cancel)
{
    CheckResult result;
    result.status = CheckStatus::NetworkError;

    std::string body;
    DWORD status = 0;
    const HttpOutcome outcome = httpGet(
        kLatestReleaseUrl,
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n",
        kMaxApiResponseBytes, cancel, body, status, result.error);

    if (outcome == HttpOutcome::Cancelled) {
        result.status = CheckStatus::Cancelled;
        result.error = "已取消。";
        return result;
    }
    if (outcome != HttpOutcome::Ok) {
        if (result.error.empty()) {
            result.error = "检查更新失败。";
        }
        return result;
    }

    if (status == 404) {
        result.status = CheckStatus::NoRelease;
        result.error = "尚未发布可用的版本。";
        return result;
    }
    if (status == 403 || status == 429) {
        result.status = CheckStatus::RateLimited;
        result.error = "GitHub 接口请求受限（HTTP " + std::to_string(status) +
                       "），请稍后再试。";
        return result;
    }
    if (status != 200) {
        result.error = "更新服务器返回 HTTP " + std::to_string(status) + "。";
        return result;
    }

    if (!parseReleaseJson(body, result.release, result.error)) {
        result.status = CheckStatus::ParseError;
        return result;
    }

    result.status = CheckStatus::Ok;
    result.hasUpdate = isVersionNewer(result.release.version, currentVersion);
    return result;
}

DownloadResult DownloadInstaller(const ReleaseAsset& asset, const std::wstring& targetPath,
                                 std::atomic<long long>& downloaded,
                                 std::atomic<long long>& total,
                                 const std::atomic_bool& cancel)
{
    DownloadResult result;
    downloaded.store(0);
    total.store(asset.size);

    ParsedUrl parsed;
    if (!parseUrl(utf8ToWide(asset.downloadUrl), parsed) || !parsed.secure ||
        !isTrustedDownloadHost(parsed.host)) {
        result.error = "下载地址无效，或不在受信任的 GitHub 域名下。";
        return result;
    }

    if (cancel.load()) {
        result.cancelled = true;
        return result;
    }

    const std::filesystem::path target(targetPath);
    const std::filesystem::path partial(targetPath + L".part");
    std::error_code ec;
    if (!target.parent_path().empty()) {
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec) {
            result.error = "无法创建下载目录：" + ec.message();
            return result;
        }
    }
    deleteQuietly(partial);

    std::string expectedDigest;
    const DigestKind digestKind = parseDigest(asset.digest, expectedDigest);
    if (digestKind == DigestKind::Malformed) {
        result.error = "更新包的内容摘要格式无法识别。";
        return result;
    }

    std::string error;
    HttpRequest request =
        openRequest(parsed, L"Accept: application/octet-stream\r\n", error);
    if (!request) {
        result.error = error;
        return result;
    }

    if (!::WinHttpSendRequest(request.request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        result.error = describeWinHttpError("无法发送下载请求", ::GetLastError());
        return result;
    }
    if (!::WinHttpReceiveResponse(request.request.get(), nullptr)) {
        result.error = describeWinHttpError("无法连接下载服务器", ::GetLastError());
        return result;
    }

    DWORD status = 0;
    if (!queryStatusCode(request.request.get(), status, error)) {
        result.error = error;
        return result;
    }
    if (status != 200) {
        result.error = "下载服务器返回 HTTP " + std::to_string(status) + "。";
        return result;
    }

    unsigned long long contentLength = 0;
    queryContentLength(request.request.get(), contentLength);
    const long long expectedBytes =
        contentLength > 0
            ? (contentLength > static_cast<unsigned long long>(kMaxDownloadBytes)
                   ? kMaxDownloadBytes + 1
                   : static_cast<long long>(contentLength))
            : asset.size;
    if (expectedBytes > kMaxDownloadBytes) {
        result.error = "更新包大小超出允许的范围。";
        return result;
    }
    total.store(expectedBytes);

    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) {
        result.error = "无法写入下载文件。";
        return result;
    }

    Sha256Hasher hasher;
    const bool verifyDigest = digestKind == DigestKind::Sha256;
    if (verifyDigest && !hasher.begin()) {
        out.close();
        deleteQuietly(partial);
        result.error = "无法初始化下载校验。";
        return result;
    }

    std::vector<char> buffer(kDownloadChunkBytes);
    unsigned long long written = 0;

    const auto fail = [&](const std::string& message) {
        out.close();
        deleteQuietly(partial);
        result.error = message;
        return result;
    };

    for (;;) {
        if (cancel.load()) {
            out.close();
            deleteQuietly(partial);
            result.cancelled = true;
            return result;
        }

        DWORD available = 0;
        if (!::WinHttpQueryDataAvailable(request.request.get(), &available)) {
            return fail(describeWinHttpError("读取下载数据失败", ::GetLastError()));
        }
        if (available == 0) {
            break;
        }

        DWORD read = 0;
        if (!::WinHttpReadData(request.request.get(), buffer.data(),
                               static_cast<DWORD>(buffer.size()), &read)) {
            return fail(describeWinHttpError("读取下载数据失败", ::GetLastError()));
        }
        if (read == 0) {
            break;
        }
        if (written + read > static_cast<unsigned long long>(kMaxDownloadBytes)) {
            return fail("更新包大小超出允许的范围。");
        }

        out.write(buffer.data(), read);
        if (!out) {
            return fail("写入下载文件失败。");
        }
        if (verifyDigest && !hasher.update(buffer.data(), read)) {
            return fail("下载校验失败。");
        }

        written += read;
        downloaded.store(static_cast<long long>(written));
    }

    out.flush();
    if (!out) {
        return fail("写入下载文件失败。");
    }
    out.close();

    if (cancel.load()) {
        deleteQuietly(partial);
        result.cancelled = true;
        return result;
    }

    if (asset.size > 0 && written != static_cast<unsigned long long>(asset.size)) {
        return fail("下载文件大小与服务器信息不一致。");
    }
    if (verifyDigest && hasher.finishHex() != expectedDigest) {
        return fail("下载内容校验失败，文件可能已损坏。");
    }

    if (!::MoveFileExW(partial.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD moveError = ::GetLastError();
        deleteQuietly(partial);
        result.error = "无法保存下载文件（错误码 " + std::to_string(moveError) + "）。";
        return result;
    }

    result.success = true;
    result.path = target.wstring();
    return result;
}

bool LaunchInstaller(const std::wstring& installerPath)
{
    if (installerPath.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(installerPath, ec)) {
        return false;
    }

    wchar_t systemDirectory[MAX_PATH] = {};
    if (::GetSystemDirectoryW(systemDirectory, MAX_PATH) == 0) {
        return false;
    }
    const std::wstring cmdPath = std::wstring(systemDirectory) + L"\\cmd.exe";

    std::wstring extension = std::filesystem::path(installerPath).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

    // Wait briefly so the running application releases its executable before
    // the installer starts replacing files. "ping" works without a console.
    std::wstring command = L"\"" + cmdPath + L"\" /d /c ping -n 3 127.0.0.1 >nul & start \"\" ";
    if (extension == L".msi") {
        command += L"msiexec.exe /i ";
    }
    command += L"\"" + installerPath + L"\"";

    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process = {};
    const std::wstring workingDirectory =
        std::filesystem::path(installerPath).parent_path().wstring();
    const BOOL created = ::CreateProcessW(
        cmdPath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, workingDirectory.empty() ? nullptr : workingDirectory.c_str(), &startup,
        &process);
    if (!created) {
        return false;
    }

    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return true;
}

} // namespace awm
