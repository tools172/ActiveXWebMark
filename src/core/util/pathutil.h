#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace awm {

/// Returns an absolute, lexically normalized form of `path`.
///
/// Uses weakly_canonical() so paths that no longer exist still resolve against
/// their nearest existing ancestor instead of failing.
std::wstring normalizePath(const std::wstring& path);

/// Returns a case-insensitive comparison key for `path`.
///
/// Used to deduplicate tabs and recent-file entries on Windows. Paths are
/// normalized and separator conventions are folded to '/'.
std::wstring pathKey(const std::wstring& path);

/// Case-insensitive path equality on Windows (normalizes both sides first).
bool samePath(const std::wstring& a, const std::wstring& b);

/// Returns the last path component (file name including extension).
std::wstring fileName(const std::wstring& path);

/// Returns the file name without its extension.
std::wstring fileStem(const std::wstring& path);

/// Returns the containing directory (empty for a bare file name).
std::wstring fileDirectory(const std::wstring& path);

/// Converts a Windows file-system path to a UTF-8 file:// URL.
///
/// Spaces, '#', '?', '%' and non-ASCII bytes are percent-encoded. Local paths
/// become `file:///C:/...`; UNC paths become `file://server/share/...`.
/// When `asDirectory` is true, a trailing '/' is appended.
std::string fileUrlFromPath(const std::wstring& path, bool asDirectory = false);

/// Parses the buffer filled by GetOpenFileNameW with OFN_ALLOWMULTISELECT.
///
/// A single selection is returned as one full path. For multiple selections
/// the first string is the directory and the following strings are file names.
/// Parsing stops at the double-NUL terminator or `maxChars`.
std::vector<std::wstring> parseMultiSelectPaths(const wchar_t* buffer, std::size_t maxChars);

} // namespace awm
