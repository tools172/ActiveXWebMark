#include "util/stringutil.h"

#include <Windows.h>

namespace awm {

std::wstring utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) {
        return {};
    }
    const int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          wide.data(), len);
    return wide;
}

std::string wideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) {
        return {};
    }
    const int len = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                          static_cast<int>(wide.size()), nullptr, 0,
                                          nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(len), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                          utf8.data(), len, nullptr, nullptr);
    return utf8;
}

} // namespace awm
