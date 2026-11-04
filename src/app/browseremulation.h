#pragma once

namespace awm {

/// Opts the executable into IE11 document mode for the hosted WebBrowser
/// control by setting HKCU\...\FeatureControl\FEATURE_BROWSER_EMULATION.
///
/// Without this, a process that hosts `Shell.Explorer` defaults to IE7/IE8
/// emulation and the X-UA-Compatible meta tag alone cannot raise it. The
/// preview relies on IE11 for rgba colors, classList and PrismJS. The call is
/// best-effort: failures (locked-down registry) leave the app functional, just
/// without preview syntax highlighting.
void EnsureBrowserEmulation();

} // namespace awm
