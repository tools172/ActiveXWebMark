#include "settings/jsonfilesettings.h"

#include <fstream>
#include <utility>

#include "util/fileutil.h"

namespace awm {

JsonFileSettings::JsonFileSettings(std::filesystem::path path) : path_(std::move(path))
{
    load();
}

std::string JsonFileSettings::get(const std::string& key, const std::string& defaultValue) const
{
    if (data_.contains(key) && data_[key].is_string()) {
        return data_[key].get<std::string>();
    }
    return defaultValue;
}

void JsonFileSettings::set(const std::string& key, const std::string& value)
{
    data_[key] = value;
    save();
}

void JsonFileSettings::load()
{
    std::ifstream in(path_);
    if (in) {
        try {
            in >> data_;
        } catch (const nlohmann::json::exception&) {
            data_ = nlohmann::json::object();
        }
    } else {
        data_ = nlohmann::json::object();
    }
}

void JsonFileSettings::save() const
{
    // Reuse the atomic writer so a crash during save cannot leave a truncated
    // settings file behind.
    std::error_code ec;
    writeFile(path_.wstring(), data_.dump(2), ec);
}

} // namespace awm
