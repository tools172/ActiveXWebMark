#include "util/fileutil.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace awm {

std::optional<std::string> readFile(const std::wstring& path, std::error_code& ec)
{
    ec.clear();

    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return std::nullopt;
    }

    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    if (in.bad()) {
        ec = std::make_error_code(std::errc::io_error);
        return std::nullopt;
    }

    // Strip a UTF-8 BOM if present.
    if (contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }

    return contents;
}

} // namespace awm
