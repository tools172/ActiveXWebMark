#pragma once

#include <optional>
#include <string>
#include <system_error>

namespace awm {

/// Reads a file into a UTF-8 string (stripping a UTF-8 BOM if present).
///
/// Returns nullopt and sets `ec` on failure.
std::optional<std::string> readFile(const std::wstring& path, std::error_code& ec);

/// Writes `utf8` to a file as UTF-8 without a BOM.
///
/// The write is atomic: content goes to a sibling temporary file first and is
/// then moved over the target. Parent directories are created when missing.
/// Returns false and sets `ec` on failure.
bool writeFile(const std::wstring& path, const std::string& utf8, std::error_code& ec);

/// Creates the parent directory of `filePath` if it does not exist.
bool ensureParentDirectory(const std::wstring& filePath, std::error_code& ec);

} // namespace awm
