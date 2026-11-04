#include "fileassociation.h"

#include <Windows.h>
#include <ShlObj.h>
#include <Shellapi.h>
#include <winternl.h>

#include <cwchar>
#include <string>
#include <utility>

namespace awm {

namespace {

constexpr wchar_t kProductName[] = L"ActiveXWebMark";
constexpr wchar_t kProgId[] = L"ActiveXWebMark.Markdown";
constexpr wchar_t kExeName[] = L"ActiveXWebMark.exe";
constexpr wchar_t kTypeName[] = L"Markdown 文档";
constexpr wchar_t kDescription[] = L"Markdown 阅读与编辑";
constexpr wchar_t kCapabilities[] = L"Software\\ActiveXWebMark\\Capabilities";

const wchar_t* const kExtensions[] = {L".md", L".markdown", L".mdown"};

std::wstring executablePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring openCommand(const std::wstring& exe)
{
    return L"\"" + exe + L"\" \"%1\"";
}

bool setString(HKEY root, const std::wstring& subKey, const wchar_t* name,
               const std::wstring& value)
{
    HKEY key = nullptr;
    if (::RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                          KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LONG result = ::RegSetValueExW(key, name, 0, REG_SZ,
                                         reinterpret_cast<const BYTE*>(value.c_str()), bytes);
    ::RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool readString(HKEY root, const std::wstring& subKey, const wchar_t* name,
                std::wstring& value)
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(root, subKey.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD bytes = 0;
    LONG result = ::RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        ::RegCloseKey(key);
        return false;
    }

    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    result = ::RegQueryValueExW(key, name, nullptr, &type,
                                reinterpret_cast<BYTE*>(buffer.data()), &bytes);
    ::RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    buffer.resize(std::wcslen(buffer.c_str()));
    value = std::move(buffer);
    return true;
}

bool keyExists(HKEY root, const wchar_t* subKey)
{
    HKEY key = nullptr;
    if (::RegOpenKeyExW(root, subKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    ::RegCloseKey(key);
    return true;
}

bool hasMachineRegistration()
{
    return keyExists(HKEY_LOCAL_MACHINE, kCapabilities);
}

bool registrationMatches(HKEY root, const std::wstring& exe)
{
    std::wstring command;
    const std::wstring commandKey =
        std::wstring(L"Software\\Classes\\") + kProgId + L"\\shell\\open\\command";
    if (!readString(root, commandKey, nullptr, command)) {
        return false;
    }
    return ::lstrcmpiW(command.c_str(), openCommand(exe).c_str()) == 0;
}

bool writeUserRegistration(const std::wstring& exe)
{
    const std::wstring command = openCommand(exe);
    const std::wstring classes = L"Software\\Classes\\";
    const std::wstring progIdKey = classes + kProgId;
    const std::wstring appKey = classes + L"Applications\\" + kExeName;

    bool ok = true;

    // ProgID: what the file type is called and which command opens it.
    ok &= setString(HKEY_CURRENT_USER, progIdKey, nullptr, kTypeName);
    ok &= setString(HKEY_CURRENT_USER, progIdKey, L"FriendlyTypeName", kTypeName);
    ok &= setString(HKEY_CURRENT_USER, progIdKey + L"\\DefaultIcon", nullptr,
                    exe + L",0");
    ok &= setString(HKEY_CURRENT_USER, progIdKey + L"\\shell\\open", L"MultiSelectModel",
                    L"Player");
    ok &= setString(HKEY_CURRENT_USER, progIdKey + L"\\shell\\open\\command", nullptr,
                    command);

    // Applications\ActiveXWebMark.exe makes the executable show up in the
    // Explorer "Open with" list.
    ok &= setString(HKEY_CURRENT_USER, appKey, L"FriendlyAppName", kProductName);
    ok &= setString(HKEY_CURRENT_USER, appKey, L"ApplicationCompany", kProductName);
    ok &= setString(HKEY_CURRENT_USER, appKey + L"\\DefaultIcon", nullptr, exe + L",0");
    ok &= setString(HKEY_CURRENT_USER, appKey + L"\\shell\\open", L"MultiSelectModel",
                    L"Player");
    ok &= setString(HKEY_CURRENT_USER, appKey + L"\\shell\\open\\command", nullptr,
                    command);

    // Capabilities + RegisteredApplications make the app appear in the
    // "Default apps" settings page.
    ok &= setString(HKEY_CURRENT_USER, kCapabilities, L"ApplicationName", kProductName);
    ok &= setString(HKEY_CURRENT_USER, kCapabilities, L"ApplicationDescription",
                    kDescription);
    ok &= setString(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", kProductName,
                    kCapabilities);

    for (const wchar_t* extension : kExtensions) {
        ok &= setString(HKEY_CURRENT_USER, appKey + L"\\SupportedTypes", extension, L"");
        ok &= setString(HKEY_CURRENT_USER, classes + extension + L"\\OpenWithProgids",
                        kProgId, L"");
        ok &= setString(HKEY_CURRENT_USER,
                        std::wstring(kCapabilities) + L"\\FileAssociations", extension,
                        kProgId);
    }

    // Let Explorer pick the new registration up without a restart.
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

bool isWindows11_22H2OrLater()
{
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return false;
    }
    const auto rtlGetVersion =
        reinterpret_cast<RtlGetVersionFn>(::GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtlGetVersion == nullptr) {
        return false;
    }

    RTL_OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = static_cast<ULONG>(sizeof(version));
    if (rtlGetVersion(&version) != 0) {
        return false;
    }
    return version.dwMajorVersion > 10 ||
           (version.dwMajorVersion == 10 && version.dwBuildNumber >= 22621);
}

} // namespace

bool ensureFileAssociationsRegistered()
{
    // The per-machine installer registration already covers the installed
    // executable. Only portable copies need a per-user registration.
    if (hasMachineRegistration()) {
        return true;
    }

    const std::wstring exe = executablePath();
    if (exe.empty()) {
        return false;
    }
    if (registrationMatches(HKEY_CURRENT_USER, exe)) {
        return true;
    }
    return writeUserRegistration(exe);
}

void openDefaultAppsSettings()
{
    std::wstring uri = L"ms-settings:defaultapps";
    // Windows 11 22H2 (with the 2023-04 cumulative update) can deep-link to a
    // specific app; Windows 10 only supports the generic page.
    if (isWindows11_22H2OrLater()) {
        uri += hasMachineRegistration() ? L"?registeredAppMachine=ActiveXWebMark"
                                        : L"?registeredAppUser=ActiveXWebMark";
    }

    const INT_PTR launched = reinterpret_cast<INT_PTR>(
        ::ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (launched > 32) {
        return;
    }

    // Fall back to the older Default Programs control panel page.
    ::ShellExecuteW(nullptr, L"open", L"control.exe", L"/name Microsoft.DefaultPrograms",
                    nullptr, SW_SHOWNORMAL);
}

} // namespace awm
