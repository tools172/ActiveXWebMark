#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#include "settings/isettings.h"

namespace awm {

/// ISettings backed by a JSON file (portable, no Windows registry dependency).
///
/// The path is a native wide path so settings continue to work when the user
/// profile (and therefore %LOCALAPPDATA%) contains non-ASCII characters.
class JsonFileSettings : public ISettings {
public:
    explicit JsonFileSettings(std::filesystem::path path);

    std::string get(const std::string& key,
                    const std::string& defaultValue = {}) const override;
    void set(const std::string& key, const std::string& value) override;

    const std::filesystem::path& path() const { return path_; }

private:
    void load();
    void save() const;

    std::filesystem::path path_;
    nlohmann::json data_;
};

} // namespace awm
