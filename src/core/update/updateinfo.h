#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace awm {

/// One downloadable file attached to a GitHub release.
struct ReleaseAsset {
    std::string name;
    std::string downloadUrl;
    /// GitHub's content digest such as "sha256:..."; empty when not provided.
    std::string digest;
    long long size = 0;
};

/// The subset of a GitHub release the updater needs.
struct ReleaseInfo {
    std::string tag;         ///< Raw tag, e.g. "v0.3.0".
    std::string version;     ///< Normalized tag without a leading "v".
    std::string name;        ///< Release title.
    std::string notes;       ///< Release body (Markdown).
    std::string releaseUrl;  ///< GitHub release page.
    std::string publishedAt; ///< ISO-8601 timestamp.
    std::vector<ReleaseAsset> assets;
};

/// Parsed semantic version used to decide whether a release is newer.
struct SemanticVersion {
    unsigned long long major = 0;
    unsigned long long minor = 0;
    unsigned long long patch = 0;
    std::string prerelease; ///< Everything after '-', empty for final releases.
};

/// Removes surrounding whitespace and a leading "v"/"V" from a release tag.
std::string normalizeVersionTag(const std::string& tag);

/// Parses "1", "1.2", "1.2.3", "v1.2.3-rc.1+build" (a leading "v" is allowed).
bool parseSemanticVersion(const std::string& text, SemanticVersion& version);

/// Compares two versions; returns <0, 0 or >0 like strcmp.
int compareSemanticVersions(const SemanticVersion& a, const SemanticVersion& b);

/// True when `candidate` is a valid semver strictly newer than `current`.
bool isVersionNewer(const std::string& candidate, const std::string& current);

/// Parses the JSON returned by the GitHub "latest release" API.
bool parseReleaseJson(const std::string& json, ReleaseInfo& release, std::string& error);

/// Picks the installer asset to download: *-setup.exe first, then .msi, then
/// any other .exe. Returns nullptr when the release has no usable asset.
const ReleaseAsset* findInstallerAsset(const ReleaseInfo& release);

/// Trims a GitHub release body for display in a dialog. Also neutralizes '<'
/// and '>' so release notes cannot inject hyperlinks into a TaskDialog.
std::wstring releaseNotesForDisplay(const std::string& notes, std::size_t maxChars);

} // namespace awm
