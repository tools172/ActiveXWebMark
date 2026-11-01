#include "settings/isettings.h"

namespace awm {

int ISettings::getInt(const std::string& key, int defaultValue) const
{
    const std::string value = get(key);
    if (value.empty()) {
        return defaultValue;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

bool ISettings::getBool(const std::string& key, bool defaultValue) const
{
    const std::string value = get(key);
    if (value == "1" || value == "true") {
        return true;
    }
    if (value == "0" || value == "false") {
        return false;
    }
    return defaultValue;
}

void ISettings::setInt(const std::string& key, int value)
{
    set(key, std::to_string(value));
}

void ISettings::setBool(const std::string& key, bool value)
{
    set(key, value ? "true" : "false");
}

} // namespace awm
