#pragma once

#include <optional>
#include <string>
#include <system_error>

namespace awm {

/// Reads a file into a UTF-8 string (stripping a UTF-8 BOM if present).
///
/// Returns nullopt and sets `ec` on failure.
std::optional<std::string> readFile(const std::wstring& path, std::error_code& ec);

} // namespace awm
