#pragma once

#include <map>
#include <string>

#include "settings/isettings.h"

namespace awm::test {

/// In-memory ISettings used by core tests instead of touching the file system.
class FakeSettings : public ISettings {
public:
    std::string get(const std::string& key,
                    const std::string& defaultValue = {}) const override
    {
        const auto it = values_.find(key);
        return it == values_.end() ? defaultValue : it->second;
    }

    void set(const std::string& key, const std::string& value) override
    {
        values_[key] = value;
    }

private:
    std::map<std::string, std::string> values_;
};

} // namespace awm::test
