#pragma once

#include <string>

namespace awm {

/// Converts a UTF-8 string to a wide (UTF-16) string.
std::wstring utf8ToWide(const std::string& utf8);

/// Converts a wide (UTF-16) string to a UTF-8 string.
std::string wideToUtf8(const std::wstring& wide);

} // namespace awm
