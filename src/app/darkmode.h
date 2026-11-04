#pragma once

#include <Windows.h>

namespace awm {

enum class ColorMode {
    Light,
    Dark,
};

/// Opts the process into the Windows 10/11 dark control rendering. This is
/// best-effort: the uxtheme exports used here are undocumented, and on systems
/// where they are unavailable the native controls simply keep their system
/// colors. Custom-drawn controls are unaffected.
void SetProcessColorMode(ColorMode mode);

/// Asks a native window (Scintilla, TreeView, ListView, dialogs, ...) to use
/// the requested Windows control theme. Safe to call repeatedly.
void ApplyColorModeToWindow(HWND hwnd, ColorMode mode);

} // namespace awm
