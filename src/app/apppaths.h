#pragma once

#include <filesystem>
#include <memory>

#include "settings/isettings.h"

namespace awm {

/// Returns `%LOCALAPPDATA%\ActiveXWebMark\settings.json`.
///
/// Falls back to a settings file beside the executable when the local app data
/// folder cannot be resolved.
std::filesystem::path settingsFilePath();

/// Creates the application settings store (creating its directory if needed).
std::unique_ptr<ISettings> createSettings();

} // namespace awm
