#include "appsettings.h"

#include <algorithm>

namespace awm {

namespace {

// Clamps a ratio read from disk so a corrupt setting cannot hide a pane.
double ratioFromPercent(int percent, double fallback)
{
    if (percent < 20 || percent > 80) {
        return fallback;
    }
    return static_cast<double>(percent) / 100.0;
}

int percentFromRatio(double ratio)
{
    return std::clamp(static_cast<int>(ratio * 100.0 + 0.5), 20, 80);
}

bool placementIsVisible(const RECT& rect)
{
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return false;
    }
    // Require at least part of the window title area to intersect a monitor.
    RECT probe{rect.left, rect.top, rect.left + 120, rect.top + 40};
    return ::MonitorFromRect(&probe, MONITOR_DEFAULTTONULL) != nullptr;
}

} // namespace

ViewSettings LoadViewSettings(ISettings& settings)
{
    ViewSettings view;
    view.showEditor = settings.getBool("ui.showEditor", view.showEditor);
    view.showNav = settings.getBool("ui.showNav", view.showNav);
    view.split = settings.get("ui.splitOrientation", "sideBySide") == "stacked"
                     ? ui::Orientation::Vertical
                     : ui::Orientation::Horizontal;
    view.sideBySideRatio =
        ratioFromPercent(settings.getInt("ui.sideBySideRatio", 55), view.sideBySideRatio);
    view.stackedRatio =
        ratioFromPercent(settings.getInt("ui.stackedRatio", 55), view.stackedRatio);
    view.navWidth = std::clamp(settings.getInt("ui.navWidth", view.navWidth), 160, 480);
    return view;
}

void SaveViewSettings(ISettings& settings, const ViewSettings& view)
{
    settings.setBool("ui.showEditor", view.showEditor);
    settings.setBool("ui.showNav", view.showNav);
    settings.set("ui.splitOrientation",
                 view.split == ui::Orientation::Vertical ? "stacked" : "sideBySide");
    settings.setInt("ui.sideBySideRatio", percentFromRatio(view.sideBySideRatio));
    settings.setInt("ui.stackedRatio", percentFromRatio(view.stackedRatio));
    settings.setInt("ui.navWidth", std::clamp(view.navWidth, 160, 480));
}

WindowPlacementState LoadWindowPlacement(ISettings& settings)
{
    WindowPlacementState placement;
    placement.x = settings.getInt("window.x", 0);
    placement.y = settings.getInt("window.y", 0);
    placement.width = settings.getInt("window.width", 0);
    placement.height = settings.getInt("window.height", 0);
    placement.maximized = settings.getBool("window.maximized", false);

    const RECT rect{placement.x, placement.y, placement.x + placement.width,
                    placement.y + placement.height};
    placement.valid = placement.width > 0 && placement.height > 0 &&
                      placementIsVisible(rect);
    return placement;
}

void SaveWindowPlacement(ISettings& settings, HWND window)
{
    if (window == nullptr) {
        return;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(window, &placement)) {
        return;
    }

    const RECT& normal = placement.rcNormalPosition;
    settings.setInt("window.x", normal.left);
    settings.setInt("window.y", normal.top);
    settings.setInt("window.width", normal.right - normal.left);
    settings.setInt("window.height", normal.bottom - normal.top);
    settings.setBool("window.maximized", placement.showCmd == SW_SHOWMAXIMIZED);
}

} // namespace awm
