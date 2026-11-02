#include "util/fileutil.h"

#include <Windows.h>

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

bool ensureParentDirectory(const std::wstring& filePath, std::error_code& ec)
{
    ec.clear();
    const std::filesystem::path parent = std::filesystem::path(filePath).parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code localError;
    std::filesystem::create_directories(parent, localError);
    if (localError && !std::filesystem::is_directory(parent)) {
        ec = localError;
        return false;
    }
    return true;
}

bool writeFile(const std::wstring& path, const std::string& utf8, std::error_code& ec)
{
    ec.clear();
    if (path.empty()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }

    if (!ensureParentDirectory(path, ec)) {
        return false;
    }

    const std::filesystem::path target(path);
    const std::filesystem::path temporary = target.wstring() + L".tmp";

    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            ec = std::make_error_code(std::errc::permission_denied);
            return false;
        }
        out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            ec = std::make_error_code(std::errc::io_error);
            return false;
        }
    }

    if (!::MoveFileExW(temporary.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        ec = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

} // namespace awm
