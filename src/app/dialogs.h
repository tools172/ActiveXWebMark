#pragma once

#include <Windows.h>

#include <string>

namespace awm {

/// Outcome of the unsaved-changes prompt.
enum class SaveDecision {
    Save,
    Discard,
    Cancel,
};

/// Asks whether to save a modified document before closing it.
SaveDecision AskSaveChanges(HWND owner, const std::wstring& documentTitle);

/// Shows a modal error dialog (TaskDialog with a MessageBox fallback).
void ShowError(HWND owner, const std::wstring& message);

/// Shows a modal information dialog.
void ShowInfo(HWND owner, const std::wstring& message);

/// Shows the About dialog (name, version, build, technology and credits).
void ShowAboutDialog(HWND owner);

} // namespace awm
