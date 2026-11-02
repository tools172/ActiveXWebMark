#include "util/pathutil.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

#include "util/stringutil.h"

namespace awm {

namespace {

// Lower-cases ASCII and BMP characters for case-insensitive path comparison.
std::wstring toLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

// Percent-encodes everything except RFC 3986 unreserved characters and the
// path separators/volume separator.
void appendPercentEncoded(std::string& out, const std::string& utf8)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    for (const unsigned char ch : utf8) {
        const bool unreserved = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                                (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' ||
                                ch == '_' || ch == '~';
        if (unreserved || ch == '/' || ch == ':') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('%');
            out.push_back(kHex[ch >> 4]);
            out.push_back(kHex[ch & 0x0F]);
        }
    }
}

} // namespace

std::wstring normalizePath(const std::wstring& path)
{
    if (path.empty()) {
        return {};
    }

    std::error_code ec;
    const std::filesystem::path input(path);
    std::filesystem::path absolute = std::filesystem::weakly_canonical(input, ec);
    if (ec || absolute.empty()) {
        ec.clear();
        absolute = std::filesystem::absolute(input, ec);
        if (ec || absolute.empty()) {
            return input.lexically_normal().wstring();
        }
    }
    return absolute.lexically_normal().wstring();
}

std::wstring pathKey(const std::wstring& path)
{
    if (path.empty()) {
        return {};
    }
    // weakly_canonical may return an empty path for non-existent relative
    // input; normalizePath falls back to lexically_normal in that case.
    return toLower(std::filesystem::path(normalizePath(path)).generic_wstring());
}

bool samePath(const std::wstring& a, const std::wstring& b)
{
    return pathKey(a) == pathKey(b);
}

std::wstring fileName(const std::wstring& path)
{
    return std::filesystem::path(path).filename().wstring();
}

std::wstring fileStem(const std::wstring& path)
{
    return std::filesystem::path(path).stem().wstring();
}

std::wstring fileDirectory(const std::wstring& path)
{
    return std::filesystem::path(path).parent_path().wstring();
}

std::string fileUrlFromPath(const std::wstring& path, bool asDirectory)
{
    if (path.empty()) {
        return {};
    }

    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(path), ec);
    if (ec || absolute.empty()) {
        absolute = std::filesystem::path(path);
    }

    std::wstring native = absolute.lexically_normal().wstring();
    // std::filesystem::path::lexically_normal() preserves a trailing separator
    // only when one was supplied; normalize it explicitly for directory URLs.
    if (asDirectory && !native.empty() && native.back() != L'\\' && native.back() != L'/') {
        native.push_back(L'\\');
    }

    const bool unc = native.size() >= 2 &&
                     (native[0] == L'\\' || native[0] == L'/') &&
                     (native[1] == L'\\' || native[1] == L'/');

    std::replace(native.begin(), native.end(), L'\\', L'/');
    const std::string utf8 = wideToUtf8(native);

    std::string url = unc ? "file:" : "file:///";
    appendPercentEncoded(url, utf8);
    return url;
}

std::vector<std::wstring> parseMultiSelectPaths(const wchar_t* buffer, std::size_t maxChars)
{
    std::vector<std::wstring> parts;
    if (buffer == nullptr || maxChars == 0) {
        return parts;
    }

    std::size_t pos = 0;
    while (pos < maxChars) {
        std::size_t length = 0;
        while (pos + length < maxChars && buffer[pos + length] != L'\0') {
            ++length;
        }
        if (length == 0) {
            break; // Empty string terminates the list (single or double NUL).
        }
        parts.emplace_back(buffer + pos, length);
        pos += length + 1;
    }

    if (parts.size() <= 1) {
        return parts;
    }

    // Multi-select: the first string is the directory, the rest are names.
    const std::filesystem::path directory(parts.front());
    std::vector<std::wstring> result;
    result.reserve(parts.size() - 1);
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const std::filesystem::path entry(parts[i]);
        result.push_back(entry.is_absolute() ? entry.wstring()
                                             : (directory / entry).wstring());
    }
    return result;
}

} // namespace awm
