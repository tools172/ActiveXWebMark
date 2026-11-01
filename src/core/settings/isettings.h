#pragma once

#include <string>

namespace awm {

/// Persistent key/value settings store.
///
/// Keys are dotted ASCII names (e.g. "editor.fontSize"); values are UTF-8
/// strings. A JSON-file implementation is provided; the interface allows other
/// backends (registry, INI) without touching the rest of the application.
class ISettings {
public:
    virtual ~ISettings() = default;

    virtual std::string get(const std::string& key,
                            const std::string& defaultValue = {}) const = 0;
    virtual void set(const std::string& key, const std::string& value) = 0;

    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    void setInt(const std::string& key, int value);
    void setBool(const std::string& key, bool value);
};

} // namespace awm
