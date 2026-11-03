#pragma once

#include <string>

namespace awm {

/// Options for wrapping rendered Markdown body HTML in a preview document.
struct PreviewHtmlOptions {
    /// Base URL (typically `file:///C:/docs/`) used to resolve relative images
    /// and links. Empty means no <base> element is emitted.
    std::string baseFileUrl;
    /// Additional CSS appended after the built-in stylesheet.
    std::string extraCss;
};

/// Escapes a string for use inside a double-quoted HTML attribute.
std::string EscapeHtmlAttribute(const std::string& value);

/// Wraps body HTML in a complete, IE11-compatible preview document with the
/// shared light typography stylesheet.
std::string BuildPreviewHtml(const std::string& bodyHtml, const PreviewHtmlOptions& options);

} // namespace awm
