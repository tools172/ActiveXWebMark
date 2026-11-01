#pragma once

#include <Windows.h>

#include <functional>
#include <memory>
#include <string>

namespace awm {

/// Callbacks from the preview view back to the host (application).
struct PreviewCallbacks {
    /// Invoked when the user scrolls the preview; `ratio` is in [0, 1].
    std::function<void(double ratio)> onScrolled;
    /// Invoked when the user clicks a link; the host decides whether to open it
    /// in the system browser.
    std::function<void(const std::string& url)> onLink;
    /// Generic message bridge (used by engine-specific JS bridge / scroll sync).
    std::function<void(const std::string& message)> onMessage;
};

/// Engine-agnostic HTML preview view.
///
/// The host only depends on this narrow interface, so the rendering engine
/// (IE WebBrowser / Miniblink / WebView2 / litehtml) can be swapped without
/// touching application code. Engine-specific capabilities are reached through
/// RunJavaScript() and the onMessage callback, not through interface methods.
class IPreviewView {
public:
    virtual ~IPreviewView() = default;

    virtual bool Create(HWND host, const RECT& rc) = 0;
    virtual void Destroy() = 0;
    virtual void SetBounds(const RECT& rc) = 0;
    virtual void Show(bool visible) = 0;

    virtual void SetHtml(const std::string& html, const std::string& baseUrl) = 0;
    virtual void SetZoom(int percent) = 0;
    /// Navigates the preview to a section anchor (for TOC navigation).
    virtual void ScrollToAnchor(const std::string& anchor) = 0;
    /// Executes JavaScript. Returns false if the engine does not support it.
    virtual bool RunJavaScript(const std::string& script) = 0;

    virtual void SetCallbacks(PreviewCallbacks callbacks) = 0;

    /// Forwards a keyboard message to the underlying control so it can handle
    /// accelerators (e.g. Ctrl+C to copy). Returns true if the message was
    /// handled and should not be processed further.
    virtual bool TranslateAccelerator(MSG* msg) = 0;

    virtual bool SupportsJavaScript() const = 0;
};

/// The set of render engines known to the factory.
enum class PreviewEngine {
    Ie,       ///< IE WebBrowser control (MSHTML) — default, single-process.
    Miniblink,
    WebView2,
    Litehtml,
};

/// Creates a preview view for the given engine.
///
/// Returns nullptr if the engine is unavailable on this system.
std::unique_ptr<IPreviewView> CreatePreviewView(PreviewEngine engine);

} // namespace awm
