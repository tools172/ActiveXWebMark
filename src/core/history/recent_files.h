#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "settings/isettings.h"

namespace awm {

/// Recently opened files, most recent first.
///
/// Pure in-memory model with JSON serialization; persistence goes through
/// ISettings so the UI and tests can use any backend. Entries are deduplicated
/// by case-insensitive path key.
class RecentFiles {
public:
    explicit RecentFiles(std::size_t pageSize = 20, std::size_t maxEntries = 100);

    /// Adds/moves `path` to the front. Empty paths are ignored.
    void add(const std::wstring& path);

    /// Removes `path` if present. Returns true when an entry was removed.
    bool remove(const std::wstring& path);

    void clear() { entries_.clear(); }

    const std::vector<std::wstring>& entries() const { return entries_; }
    std::size_t size() const { return entries_.size(); }
    std::size_t pageSize() const { return pageSize_; }

    /// Returns up to `count` entries starting at `offset`.
    std::vector<std::wstring> page(std::size_t offset, std::size_t count) const;

    /// Returns the first page shown directly in the File menu.
    std::vector<std::wstring> menuPage() const { return page(0, pageSize_); }

    std::string toJson() const;
    static RecentFiles fromJson(const std::string& json, std::size_t pageSize = 20,
                                std::size_t maxEntries = 100);

    void load(const ISettings& settings, const std::string& key);
    void save(ISettings& settings, const std::string& key) const;

private:
    void appendUnique(const std::wstring& path);
    void trimToCapacity();

    std::size_t pageSize_;
    std::size_t maxEntries_;
    std::vector<std::wstring> entries_;
};

} // namespace awm
