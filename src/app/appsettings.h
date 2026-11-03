#pragma once

#include <Windows.h>

#include <string>
#include <vector>

#include "settings/isettings.h"
#include "viewstate.h"

namespace awm {

/// Persisted window geometry (normal rect + maximized state).
struct WindowPlacementState {
    bool valid = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool maximized = false;
};

ViewSettings LoadViewSettings(ISettings& settings);
void SaveViewSettings(ISettings& settings, const ViewSettings& view);

WindowPlacementState LoadWindowPlacement(ISettings& settings);
void SaveWindowPlacement(ISettings& settings, HWND window);

/// Tabs that were open when the application last exited. Only regular files
/// are restored; tabs without a path cannot exist in the current UI.
struct SessionState {
    std::vector<std::wstring> files;
    std::wstring activeFile;
};

SessionState LoadSessionState(ISettings& settings);
void SaveSessionState(ISettings& settings, const std::vector<std::wstring>& files,
                      const std::wstring& activeFile);

} // namespace awm
