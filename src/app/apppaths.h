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

/// Directory used for downloaded update installers.
///
/// Lives next to the settings file, so it follows the same
/// %LOCALAPPDATA%\ActiveXWebMark (or AWM_SETTINGS_DIR) location.
std::filesystem::path updateDirectory();

/// Directory containing user theme files (`*.awmtheme.json`). Lives next to
/// the settings file, so AWM_SETTINGS_DIR isolates tests and portable runs.
std::filesystem::path themesDirectory();

} // namespace awm
