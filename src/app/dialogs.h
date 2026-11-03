#pragma once

#include <Windows.h>

#include <string>

#include "update/updateinfo.h"

namespace awm {

/// Outcome of the unsaved-changes prompt.
enum class SaveDecision {
    Save,
    Discard,
    Cancel,
};

/// Outcome of the "new version available" prompt.
enum class UpdatePromptChoice {
    Install,
    OpenPage,
    Later,
    Ignore,
};

/// Asks whether to save a modified document before closing it.
SaveDecision AskSaveChanges(HWND owner, const std::wstring& documentTitle);

/// Offers to upgrade to `release`. `hasInstaller` selects "立即升级" versus
/// "打开下载页面" as the primary action.
UpdatePromptChoice AskUpdatePrompt(HWND owner, const std::wstring& currentVersion,
                                   const ReleaseInfo& release, bool hasInstaller);

/// Shows a modal error dialog (TaskDialog with a MessageBox fallback).
void ShowError(HWND owner, const std::wstring& message);

/// Shows a modal information dialog.
void ShowInfo(HWND owner, const std::wstring& message);

/// Shows the About dialog (name, version, build, technology and credits).
void ShowAboutDialog(HWND owner);

} // namespace awm
