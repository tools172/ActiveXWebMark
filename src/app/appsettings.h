#pragma once

#include <Windows.h>

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

} // namespace awm
