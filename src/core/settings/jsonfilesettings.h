#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "settings/isettings.h"

namespace awm {

/// ISettings backed by a JSON file (portable, no Windows registry dependency).
class JsonFileSettings : public ISettings {
public:
    explicit JsonFileSettings(std::string path);

    std::string get(const std::string& key,
                    const std::string& defaultValue = {}) const override;
    void set(const std::string& key, const std::string& value) override;

private:
    void load();
    void save() const;

    std::string path_;
    nlohmann::json data_;
};

} // namespace awm
