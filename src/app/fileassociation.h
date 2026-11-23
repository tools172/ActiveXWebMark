#pragma once

namespace awm {

/// Makes sure this executable is registered as a Markdown handler.
///
/// The installer registers the application under HKEY_LOCAL_MACHINE; this
/// provides the same registration for portable copies, writing under
/// HKEY_CURRENT_USER instead. When a per-machine registration is present,
/// stale per-user entries left by a portable copy are removed so they cannot
/// mask the machine settings (including the file-type icon).
///
/// Returns false when the registration could not be written.
bool ensureFileAssociationsRegistered();

/// Opens the Windows "Default apps" settings page.
///
/// Windows 10 does not let an application change the default handler by
/// itself, so this only takes the user to the page where they can select
/// ActiveXWebMark for Markdown files.
void openDefaultAppsSettings();

} // namespace awm
