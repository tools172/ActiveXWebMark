#include "settings/jsonfilesettings.h"

#include <fstream>
#include <utility>

namespace awm {

JsonFileSettings::JsonFileSettings(std::string path) : path_(std::move(path))
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
    std::ofstream out(path_);
    if (out) {
        out << data_.dump(2);
    }
}

} // namespace awm
