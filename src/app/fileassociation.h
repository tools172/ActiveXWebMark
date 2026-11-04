#pragma once

namespace awm {

/// Makes sure this executable is registered as a Markdown handler.
///
/// The installer registers the application under HKEY_LOCAL_MACHINE; this
/// provides the same registration for portable copies, writing under
/// HKEY_CURRENT_USER instead. Does nothing when a per-machine registration is
/// already present, so an installed copy does not add a duplicate entry.
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
