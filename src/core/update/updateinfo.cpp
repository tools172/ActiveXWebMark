#include "updateinfo.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "util/stringutil.h"

namespace awm {

namespace {

bool isAsciiSpace(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

std::string trimAscii(const std::string& text)
{
    std::size_t begin = 0;
    while (begin < text.size() && isAsciiSpace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && isAsciiSpace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool isDigits(std::string_view text)
{
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(),
                       [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

bool isPrereleaseIdentifier(std::string_view text)
{
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '-';
    });
}

std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(separator, start);
        const std::size_t length =
            end == std::string_view::npos ? std::string_view::npos : end - start;
        parts.push_back(text.substr(start, length));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

bool parseUnsigned(std::string_view text, unsigned long long& value)
{
    if (!isDigits(text)) {
        return false;
    }
    unsigned long long result = 0;
    for (const char ch : text) {
        const unsigned digit = static_cast<unsigned>(ch - '0');
        if (result > (std::numeric_limits<unsigned long long>::max() - digit) / 10ULL) {
            return false;
        }
        result = result * 10ULL + digit;
    }
    value = result;
    return true;
}

// Semver prerelease comparison: numeric identifiers compare numerically and
// sort before alphanumeric ones; a release without a prerelease sorts last.
int comparePrereleaseIdentifier(std::string_view a, std::string_view b)
{
    const bool aNumeric = isDigits(a);
    const bool bNumeric = isDigits(b);
    if (aNumeric && bNumeric) {
        std::string_view aTrimmed = a;
        std::string_view bTrimmed = b;
        while (aTrimmed.size() > 1 && aTrimmed.front() == '0') {
            aTrimmed.remove_prefix(1);
        }
        while (bTrimmed.size() > 1 && bTrimmed.front() == '0') {
            bTrimmed.remove_prefix(1);
        }
        if (aTrimmed.size() != bTrimmed.size()) {
            return aTrimmed.size() < bTrimmed.size() ? -1 : 1;
        }
        const int result = aTrimmed.compare(bTrimmed);
        return result < 0 ? -1 : (result > 0 ? 1 : 0);
    }
    if (aNumeric != bNumeric) {
        return aNumeric ? -1 : 1;
    }
    const int result = a.compare(b);
    return result < 0 ? -1 : (result > 0 ? 1 : 0);
}

int comparePrerelease(const std::string& a, const std::string& b)
{
    if (a.empty() && b.empty()) {
        return 0;
    }
    if (a.empty()) {
        return 1; // A final release is newer than any prerelease.
    }
    if (b.empty()) {
        return -1;
    }

    const std::vector<std::string_view> left = split(a, '.');
    const std::vector<std::string_view> right = split(b, '.');
    const std::size_t common = std::min(left.size(), right.size());
    for (std::size_t i = 0; i < common; ++i) {
        const int result = comparePrereleaseIdentifier(left[i], right[i]);
        if (result != 0) {
            return result;
        }
    }
    if (left.size() == right.size()) {
        return 0;
    }
    return left.size() < right.size() ? -1 : 1;
}

std::string jsonString(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

long long jsonInt64(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end()) {
        return 0;
    }
    if (it->is_number_unsigned()) {
        const unsigned long long value = it->get<unsigned long long>();
        return value > static_cast<unsigned long long>(std::numeric_limits<long long>::max())
                   ? std::numeric_limits<long long>::max()
                   : static_cast<long long>(value);
    }
    if (it->is_number_integer()) {
        const long long value = it->get<long long>();
        return value < 0 ? 0 : value;
    }
    return 0;
}

std::string asciiLower(const std::string& text)
{
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower;
}

bool endsWith(const std::string& text, std::string_view suffix)
{
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

std::string normalizeVersionTag(const std::string& tag)
{
    const std::string trimmed = trimAscii(tag);
    if (trimmed.size() > 1 && (trimmed[0] == 'v' || trimmed[0] == 'V') &&
        std::isdigit(static_cast<unsigned char>(trimmed[1])) != 0) {
        return trimmed.substr(1);
    }
    return trimmed;
}

bool parseSemanticVersion(const std::string& text, SemanticVersion& version)
{
    std::string remaining = normalizeVersionTag(text);
    if (remaining.empty()) {
        return false;
    }

    // Build metadata never participates in precedence.
    const std::size_t plus = remaining.find('+');
    if (plus != std::string::npos) {
        remaining.erase(plus);
    }

    std::string core = remaining;
    std::string prerelease;
    const std::size_t dash = remaining.find('-');
    if (dash != std::string::npos) {
        core = remaining.substr(0, dash);
        prerelease = remaining.substr(dash + 1);
        if (prerelease.empty()) {
            return false;
        }
        const std::vector<std::string_view> identifiers = split(prerelease, '.');
        if (std::any_of(identifiers.begin(), identifiers.end(),
                        [](std::string_view identifier) {
                            return !isPrereleaseIdentifier(identifier);
                        })) {
            return false;
        }
    }

    const std::vector<std::string_view> components = split(core, '.');
    if (components.empty() || components.size() > 3) {
        return false;
    }

    unsigned long long values[3] = {0, 0, 0};
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (!parseUnsigned(components[i], values[i])) {
            return false;
        }
    }

    version.major = values[0];
    version.minor = values[1];
    version.patch = values[2];
    version.prerelease = std::move(prerelease);
    return true;
}

int compareSemanticVersions(const SemanticVersion& a, const SemanticVersion& b)
{
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch ? -1 : 1;
    }
    return comparePrerelease(a.prerelease, b.prerelease);
}

bool isVersionNewer(const std::string& candidate, const std::string& current)
{
    SemanticVersion candidateVersion;
    SemanticVersion currentVersion;
    if (!parseSemanticVersion(candidate, candidateVersion) ||
        !parseSemanticVersion(current, currentVersion)) {
        return false;
    }
    return compareSemanticVersions(candidateVersion, currentVersion) > 0;
}

bool parseReleaseJson(const std::string& json, ReleaseInfo& release, std::string& error)
{
    nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (root.is_discarded()) {
        error = "更新信息不是有效的 JSON。";
        return false;
    }
    if (!root.is_object()) {
        error = "更新信息的格式不正确。";
        return false;
    }

    release = ReleaseInfo{};
    release.tag = jsonString(root, "tag_name");
    release.version = normalizeVersionTag(release.tag);
    if (release.tag.empty() || release.version.empty()) {
        error = "更新信息缺少版本标签。";
        return false;
    }

    SemanticVersion parsed;
    if (!parseSemanticVersion(release.version, parsed)) {
        error = "无法识别发布版本号：" + release.tag;
        return false;
    }

    release.name = jsonString(root, "name");
    release.notes = jsonString(root, "body");
    release.releaseUrl = jsonString(root, "html_url");
    release.publishedAt = jsonString(root, "published_at");

    const auto assets = root.find("assets");
    if (assets != root.end() && assets->is_array()) {
        release.assets.reserve(assets->size());
        for (const nlohmann::json& item : *assets) {
            if (!item.is_object()) {
                continue;
            }

            ReleaseAsset asset;
            asset.name = jsonString(item, "name");
            asset.downloadUrl = jsonString(item, "browser_download_url");
            asset.digest = jsonString(item, "digest");
            asset.size = jsonInt64(item, "size");
            if (!asset.downloadUrl.empty()) {
                release.assets.push_back(std::move(asset));
            }
        }
    }

    return true;
}

const ReleaseAsset* findInstallerAsset(const ReleaseInfo& release)
{
    const ReleaseAsset* setup = nullptr;
    const ReleaseAsset* msi = nullptr;
    const ReleaseAsset* exe = nullptr;

    for (const ReleaseAsset& asset : release.assets) {
        if (asset.downloadUrl.empty()) {
            continue;
        }
        const std::string name = asciiLower(asset.name);
        if (endsWith(name, ".msi")) {
            if (msi == nullptr) {
                msi = &asset;
            }
        } else if (endsWith(name, ".exe")) {
            if (name.find("setup") != std::string::npos) {
                if (setup == nullptr) {
                    setup = &asset;
                }
            } else if (exe == nullptr) {
                exe = &asset;
            }
        }
    }

    if (setup != nullptr) {
        return setup;
    }
    if (msi != nullptr) {
        return msi;
    }
    return exe;
}

std::wstring releaseNotesForDisplay(const std::string& notes, std::size_t maxChars)
{
    std::wstring text = utf8ToWide(notes);

    std::size_t begin = 0;
    while (begin < text.size() &&
           (text[begin] == L' ' || text[begin] == L'\t' || text[begin] == L'\r' ||
            text[begin] == L'\n')) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && (text[end - 1] == L' ' || text[end - 1] == L'\t' ||
                           text[end - 1] == L'\r' || text[end - 1] == L'\n')) {
        --end;
    }
    text = text.substr(begin, end - begin);

    // TaskDialog renders <a href="..."> when hyperlinks are enabled. Release
    // notes are plain Markdown, so neutralize angle brackets defensively.
    for (wchar_t& ch : text) {
        if (ch == L'<') {
            ch = L'＜';
        } else if (ch == L'>') {
            ch = L'＞';
        }
    }

    if (maxChars > 0 && text.size() > maxChars) {
        std::size_t cut = maxChars;
        // Do not split a UTF-16 surrogate pair in half.
        if (cut > 0 && cut < text.size() && text[cut - 1] >= 0xD800 &&
            text[cut - 1] <= 0xDBFF) {
            --cut;
        }
        text = text.substr(0, cut) + L"\n…";
    }
    return text;
}

} // namespace awm
