#include "theme/theme_import_internal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "miniz.h"
#include "theme/theme_json.h"
#include "util/fileutil.h"

namespace awm::theme::detail {

namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxArchiveBytes = 32u * 1024u * 1024u;
constexpr std::size_t kMaxEntryBytes = 8u * 1024u * 1024u;
constexpr std::size_t kMaxTotalExtracted = 32u * 1024u * 1024u;
constexpr std::size_t kMaxEntries = 2048;

std::string NormalizeArchivePath(const std::string& rawName)
{
    std::string name;
    name.reserve(rawName.size());
    for (const char ch : rawName) {
        name.push_back(ch == '\\' ? '/' : ch);
    }
    return name;
}

bool HasSafeRelativePath(const std::string& rawName)
{
    if (rawName.empty()) {
        return false;
    }
    const std::string name = NormalizeArchivePath(rawName);
    if (name.front() == '/' || name.find(':') != std::string::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= name.size()) {
        const std::size_t slash = name.find('/', start);
        const std::string_view segment(
            name.data() + start, slash == std::string::npos ? name.size() - start
                                                            : slash - start);
        if (segment == "..") {
            return false;
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return true;
}

std::filesystem::path MakeTempDirectory()
{
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("awm-vsix-" + std::to_string(stamp) + "-" + std::to_string(counter++));
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return {};
    }
    return path;
}

struct TempDirectory {
    std::filesystem::path path;
    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

bool ExtractArchive(const std::vector<std::uint8_t>& archive,
                    const std::filesystem::path& destination, std::string& error)
{
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, archive.data(), archive.size(), 0)) {
        error = "VSIX 不是有效的 ZIP 文件。";
        return false;
    }

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    if (count > kMaxEntries) {
        mz_zip_reader_end(&zip);
        error = "VSIX 条目过多。";
        return false;
    }

    std::size_t totalExtracted = 0;
    bool ok = true;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            error = "VSIX ZIP 条目损坏。";
            ok = false;
            break;
        }
        if (stat.m_is_directory != 0) {
            continue;
        }
        const std::string name = NormalizeArchivePath(stat.m_filename);
        if (!HasSafeRelativePath(name)) {
            error = "VSIX 包含不安全的路径：" + name;
            ok = false;
            break;
        }
        if (stat.m_uncomp_size > kMaxEntryBytes) {
            error = "VSIX 条目解压后过大：" + name;
            ok = false;
            break;
        }
        if (totalExtracted + static_cast<std::size_t>(stat.m_uncomp_size) >
            kMaxTotalExtracted) {
            error = "VSIX 解压总量超过 32 MB。";
            ok = false;
            break;
        }

        std::vector<std::uint8_t> content(
            static_cast<std::size_t>(stat.m_uncomp_size));
        if (!mz_zip_reader_extract_to_mem(&zip, i, content.data(), content.size(), 0)) {
            error = "VSIX 条目解压失败：" + name;
            ok = false;
            break;
        }
        totalExtracted += content.size();

        const std::filesystem::path target = destination / name;
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec) {
            error = "无法创建 VSIX 临时目录：" + ec.message();
            ok = false;
            break;
        }
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "无法写入 VSIX 临时文件：" + target.string();
            ok = false;
            break;
        }
        out.write(reinterpret_cast<const char*>(content.data()),
                  static_cast<std::streamsize>(content.size()));
        out.close();
    }

    mz_zip_reader_end(&zip);
    return ok;
}

std::string ReadString(const Json& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

void ApplyPackageMetadata(ImportedTheme& theme, const Json& package,
                          const std::string& label)
{
    const std::string publisher = ReadString(package, "publisher");
    const std::string name = ReadString(package, "name");
    const std::string version = ReadString(package, "version");
    const std::string license = ReadString(package, "license");
    const std::string displayName = ReadString(package, "displayName");

    if (theme.source.name.empty() && !label.empty()) {
        theme.source.name = label;
    }
    if (theme.source.name.empty() && !displayName.empty()) {
        theme.source.name = displayName;
    }
    if (theme.source.author.empty()) {
        theme.source.author = publisher;
    }
    if (theme.source.license.empty()) {
        theme.source.license = license;
    }
    if (!publisher.empty() || !name.empty()) {
        std::string source = "VS Code Marketplace: ";
        source += publisher;
        if (!publisher.empty() && !name.empty()) {
            source += ".";
        }
        source += name;
        if (!version.empty()) {
            source += "@" + version;
        }
        theme.source.source = source;
    }
}

std::optional<std::filesystem::path> ResolveThemePath(const std::filesystem::path& packageDir,
                                                      const std::string& relative)
{
    std::filesystem::path path = packageDir / NormalizeArchivePath(relative);
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return path;
    }
    if (!path.has_extension()) {
        std::filesystem::path candidate = path;
        candidate += ".json";
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace

ImportResult ImportVsixFile(const std::filesystem::path& path)
{
    ImportResult result;
    std::error_code ec;
    const std::optional<std::string> raw = readFile(path.wstring(), ec);
    if (!raw) {
        result.error = "无法读取 VSIX：" + path.string() + " (" + ec.message() + ")";
        return result;
    }
    if (raw->size() > kMaxArchiveBytes) {
        result.error = "VSIX 文件超过 32 MB。";
        return result;
    }

    const std::vector<std::uint8_t> archive(raw->begin(), raw->end());
    TempDirectory temp{MakeTempDirectory()};
    if (temp.path.empty()) {
        result.error = "无法创建临时目录。";
        return result;
    }

    if (!ExtractArchive(archive, temp.path, result.error)) {
        return result;
    }

    const std::filesystem::path packagePath = temp.path / "extension" / "package.json";
    std::optional<std::string> packageText = readFile(packagePath.wstring(), ec);
    if (!packageText) {
        packageText = readFile((temp.path / "package.json").wstring(), ec);
    }
    if (!packageText) {
        result.error = "VSIX 中找不到 extension/package.json。";
        return result;
    }

    const Json package = Json::parse(StripJsonComments(*packageText), nullptr, false);
    if (package.is_discarded() || !package.is_object()) {
        result.error = "VSIX 的 package.json 不是有效 JSON。";
        return result;
    }
    const std::filesystem::path packageDir =
        std::filesystem::exists(temp.path / "extension", ec)
            ? temp.path / "extension"
            : temp.path;

    std::vector<std::pair<std::string, std::string>> candidates;  // label, path
    if (const auto contributes = package.find("contributes");
        contributes != package.end() && contributes->is_object()) {
        if (const auto themes = contributes->find("themes");
            themes != contributes->end() && themes->is_array()) {
            for (const auto& item : *themes) {
                if (!item.is_object()) {
                    continue;
                }
                const std::string label = ReadString(item, "label");
                const std::string themePath = ReadString(item, "path");
                if (!themePath.empty()) {
                    candidates.emplace_back(label, themePath);
                }
            }
        }
    }

    if (candidates.empty()) {
        // Fallback: import every JSON/JSONC under extension/themes.
        std::error_code walkError;
        const std::filesystem::path themesDir = packageDir / "themes";
        if (std::filesystem::exists(themesDir, walkError)) {
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(themesDir, walkError)) {
                if (walkError) {
                    break;
                }
                if (!entry.is_regular_file(walkError) || walkError) {
                    walkError.clear();
                    continue;
                }
                const std::string extension = entry.path().extension().string();
                if (extension == ".json" || extension == ".jsonc") {
                    candidates.emplace_back(
                        entry.path().stem().string(),
                        std::filesystem::relative(entry.path(), packageDir, walkError)
                            .generic_string());
                }
            }
        }
    }

    if (candidates.empty()) {
        result.error = "VSIX 中没有找到可导入的主题。";
        return result;
    }

    for (const auto& [label, relative] : candidates) {
        const std::optional<std::filesystem::path> themePath =
            ResolveThemePath(packageDir, relative);
        if (!themePath) {
            result.warnings.push_back("VSIX 主题文件不存在：" + relative);
            continue;
        }
        ImportResult imported = ImportVscodeThemeFile(*themePath);
        if (!imported.error.empty()) {
            result.warnings.push_back(imported.error);
            continue;
        }
        for (ImportedTheme& theme : imported.themes) {
            ApplyPackageMetadata(theme, package, label);
            result.themes.push_back(std::move(theme));
        }
        result.warnings.insert(result.warnings.end(), imported.warnings.begin(),
                               imported.warnings.end());
    }

    if (result.themes.empty()) {
        result.error = "VSIX 中的主题都无法解析。";
    }
    return result;
}

} // namespace awm::theme::detail
