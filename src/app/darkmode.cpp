#include "darkmode.h"

#include <uxtheme.h>

namespace awm {

namespace {

// Undocumented uxtheme exports. Values are from uxtheme.dll's ordinal table,
// which has been stable since Windows 10 1809. If the exports are missing the
// calls become no-ops and native controls keep their system colors.
using SetPreferredAppModeFn = int(WINAPI*)(int);
using FlushMenuThemesFn = void(WINAPI*)();
using AllowDarkModeForWindowFn = BOOL(WINAPI*)(HWND, BOOL);

struct DarkModeApi {
    SetPreferredAppModeFn setPreferredAppMode = nullptr;
    FlushMenuThemesFn flushMenuThemes = nullptr;
    AllowDarkModeForWindowFn allowDarkModeForWindow = nullptr;
};

const DarkModeApi& darkModeApi()
{
    static const DarkModeApi api = [] {
        DarkModeApi result;
        const HMODULE module = ::LoadLibraryW(L"uxtheme.dll");
        if (module == nullptr) {
            return result;
        }
        result.setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFn>(
            ::GetProcAddress(module, MAKEINTRESOURCEA(135)));
        result.flushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(
            ::GetProcAddress(module, MAKEINTRESOURCEA(136)));
        result.allowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindowFn>(
            ::GetProcAddress(module, MAKEINTRESOURCEA(133)));
        // Intentionally keep the module loaded for the process lifetime.
        return result;
    }();
    return api;
}

bool isDark(ColorMode mode)
{
    return mode == ColorMode::Dark;
}

} // namespace

void SetProcessColorMode(ColorMode mode)
{
    const DarkModeApi& api = darkModeApi();
    if (api.setPreferredAppMode != nullptr) {
        // 2 = ForceDark, 3 = ForceLight (0 = default, 1 = allow dark).
        api.setPreferredAppMode(isDark(mode) ? 2 : 3);
    }
    if (api.flushMenuThemes != nullptr) {
        api.flushMenuThemes();
    }
}

void ApplyColorModeToWindow(HWND hwnd, ColorMode mode)
{
    if (hwnd == nullptr) {
        return;
    }

    const DarkModeApi& api = darkModeApi();
    if (api.allowDarkModeForWindow != nullptr) {
        api.allowDarkModeForWindow(hwnd, isDark(mode) ? TRUE : FALSE);
    }

    // DarkMode_Explorer gives native controls (scrollbars, headers, tooltips,
    // list/tree parts) their dark variants. "Explorer" is the normal light
    // theme; passing nullptr would leave the control at its class default.
    ::SetWindowTheme(hwnd, isDark(mode) ? L"DarkMode_Explorer" : L"Explorer", nullptr);

    // Theme changes are handled in WM_THEMECHANGED; force a non-client repaint
    // as well so native scrollbars pick up the new theme immediately.
    ::SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
}

} // namespace awm
