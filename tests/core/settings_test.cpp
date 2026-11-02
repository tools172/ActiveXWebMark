#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "settings/jsonfilesettings.h"

using namespace awm;

namespace {

// A unique temp path for this test run, removed on destruction.
class TempSettingsFile {
public:
    TempSettingsFile()
        : path_(std::filesystem::temp_directory_path() / "awm_settings_test.json")
    {
        // Remove any stale file from a previous (possibly crashed) run.
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    ~TempSettingsFile()
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST(JsonFileSettings, GetReturnsDefaultWhenAbsent)
{
    TempSettingsFile file;
    JsonFileSettings settings(file.path());
    EXPECT_EQ(settings.get("missing"), "");
    EXPECT_EQ(settings.get("missing", "fallback"), "fallback");
}

TEST(JsonFileSettings, SetAndGetRoundTrip)
{
    TempSettingsFile file;
    JsonFileSettings settings(file.path());
    settings.set("editor.fontSize", "14");
    EXPECT_EQ(settings.get("editor.fontSize"), "14");
}

TEST(JsonFileSettings, TypedAccessors)
{
    TempSettingsFile file;
    JsonFileSettings settings(file.path());
    settings.setInt("zoom", 120);
    settings.setBool("wrap", true);

    EXPECT_EQ(settings.getInt("zoom"), 120);
    EXPECT_TRUE(settings.getBool("wrap"));
    EXPECT_EQ(settings.getInt("missing", 90), 90);
    EXPECT_FALSE(settings.getBool("missing", false));
}

TEST(JsonFileSettings, PersistsToDisk)
{
    TempSettingsFile file;
    {
        JsonFileSettings settings(file.path());
        settings.set("key", "value");
    }
    JsonFileSettings reloaded(file.path());
    EXPECT_EQ(reloaded.get("key"), "value");
}
