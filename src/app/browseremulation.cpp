#include "browseremulation.h"

#include <Windows.h>

namespace awm {

void EnsureBrowserEmulation()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
    if (length == 0 || length >= ARRAYSIZE(modulePath)) {
        return;
    }

    const wchar_t* fileName = modulePath;
    for (const wchar_t* cursor = modulePath; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'/') {
            fileName = cursor + 1;
        }
    }
    if (*fileName == L'\0') {
        return;
    }

    static constexpr wchar_t kKey[] =
        L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\"
        L"FEATURE_BROWSER_EMULATION";

    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0,
                          KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }

    // 11001 (0x2AF9): IE11 edge mode, independent of the document's DOCTYPE.
    const DWORD mode = 11001;
    DWORD existing = 0;
    DWORD existingSize = sizeof(existing);
    DWORD existingType = 0;
    if (::RegQueryValueExW(key, fileName, nullptr, &existingType,
                           reinterpret_cast<BYTE*>(&existing),
                           &existingSize) == ERROR_SUCCESS &&
        existingType == REG_DWORD && existing >= mode) {
        ::RegCloseKey(key);
        return;
    }

    ::RegSetValueExW(key, fileName, 0, REG_DWORD,
                     reinterpret_cast<const BYTE*>(&mode), sizeof(mode));
    ::RegCloseKey(key);
}

} // namespace awm
