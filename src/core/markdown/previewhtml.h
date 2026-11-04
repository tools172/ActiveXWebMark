#pragma once

#include <string>
#include <string_view>

#include "theme/theme_catalog.h"

namespace awm {

/// Options for wrapping rendered Markdown body HTML in a preview document.
struct PreviewHtmlOptions {
    /// Base URL (typically `file:///C:/docs/`) used to resolve relative images
    /// and links. Empty means no <base> element is emitted.
    std::string baseFileUrl;
    /// Additional CSS appended after the theme stylesheet.
    std::string extraCss;
    /// Theme used for typography, colors and Prism token colors. Null selects
    /// the default GitHub Light theme.
    const theme::ThemeDefinition* theme = nullptr;
    /// Inline JavaScript inserted after the rendered body (the embedded Prism
    /// bundle). Empty disables syntax highlighting. The caller keeps ownership
    /// of the string; it is copied into the generated document.
    std::string_view inlineScript;
};

/// Escapes a string for use inside a double-quoted HTML attribute.
std::string EscapeHtmlAttribute(const std::string& value);

/// Escapes every case-insensitive `</script` in inline JavaScript to
/// `<\/script`, so the script cannot terminate its own tag early.
std::string EscapeInlineScript(std::string_view script);

/// Wraps body HTML in a complete, IE11-compatible preview document.
///
/// The stylesheet is generated from `options.theme`. When `inlineScript` is
/// non-empty and the body contains a `language-*` code block, the document
/// also receives the embedded Prism bundle plus the IE11 compatibility patch
/// and per-block highlighting runner.
std::string BuildPreviewHtml(const std::string& bodyHtml, const PreviewHtmlOptions& options);

} // namespace awm
