#include "history/recent_files.h"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

#include "util/pathutil.h"
#include "util/stringutil.h"

namespace awm {

RecentFiles::RecentFiles(std::size_t pageSize, std::size_t maxEntries)
    : pageSize_(pageSize == 0 ? 20 : pageSize)
    , maxEntries_(maxEntries)
{
}

void RecentFiles::add(const std::wstring& path)
{
    if (path.empty()) {
        return;
    }

    const std::wstring key = pathKey(path);
    std::wstring stored = normalizePath(path);

    // If the file is already listed, keep its existing spelling (usually the
    // path as the user originally opened it) and just move it to the front.
    const auto existing = std::find_if(entries_.begin(), entries_.end(),
                                       [&key](const std::wstring& entry) {
                                           return pathKey(entry) == key;
                                       });
    if (existing != entries_.end()) {
        stored = *existing;
        entries_.erase(existing);
    }

    entries_.insert(entries_.begin(), std::move(stored));
    trimToCapacity();
}

bool RecentFiles::remove(const std::wstring& path)
{
    if (path.empty()) {
        return false;
    }
    const std::wstring key = pathKey(path);
    const auto newEnd = std::remove_if(entries_.begin(), entries_.end(),
                                       [&key](const std::wstring& entry) {
                                           return pathKey(entry) == key;
                                       });
    if (newEnd == entries_.end()) {
        return false;
    }
    entries_.erase(newEnd, entries_.end());
    return true;
}

std::vector<std::wstring> RecentFiles::page(std::size_t offset, std::size_t count) const
{
    if (offset >= entries_.size() || count == 0) {
        return {};
    }
    const std::size_t end = std::min(entries_.size(), offset + count);
    return std::vector<std::wstring>(entries_.begin() + static_cast<std::ptrdiff_t>(offset),
                                     entries_.begin() + static_cast<std::ptrdiff_t>(end));
}

std::string RecentFiles::toJson() const
{
    nlohmann::json array = nlohmann::json::array();
    for (const std::wstring& entry : entries_) {
        array.push_back(wideToUtf8(entry));
    }
    return array.dump();
}

RecentFiles RecentFiles::fromJson(const std::string& json, std::size_t pageSize,
                                  std::size_t maxEntries)
{
    RecentFiles result(pageSize, maxEntries);
    if (json.empty()) {
        return result;
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception&) {
        return result;
    }
    if (!parsed.is_array()) {
        return result;
    }

    for (const auto& value : parsed) {
        if (value.is_string()) {
            result.appendUnique(utf8ToWide(value.get<std::string>()));
        }
    }
    return result;
}

void RecentFiles::load(const ISettings& settings, const std::string& key)
{
    *this = fromJson(settings.get(key), pageSize_, maxEntries_);
}

void RecentFiles::save(ISettings& settings, const std::string& key) const
{
    settings.set(key, toJson());
}

void RecentFiles::appendUnique(const std::wstring& path)
{
    if (path.empty() || entries_.size() >= maxEntries_) {
        return;
    }
    const std::wstring key = pathKey(path);
    const bool exists = std::any_of(entries_.begin(), entries_.end(),
                                    [&key](const std::wstring& entry) {
                                        return pathKey(entry) == key;
                                    });
    if (!exists) {
        entries_.push_back(normalizePath(path));
    }
}

void RecentFiles::trimToCapacity()
{
    if (entries_.size() > maxEntries_) {
        entries_.resize(maxEntries_);
    }
}

} // namespace awm
