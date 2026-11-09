#pragma once

#include <Windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace awm {

/// Callbacks from the preview view back to the host (application).
struct PreviewCallbacks {
    /// Invoked when the user scrolls the preview; `ratio` is in [0, 1].
    std::function<void(double ratio)> onScrolled;
    /// Invoked when the user clicks a link; the host decides whether to open it
    /// in the system browser.
    std::function<void(const std::string& url)> onLink;
    /// Invoked when the preview scrolls; `sourceLine` is the (fractional)
    /// Markdown source line at the top of the viewport.
    std::function<void(double sourceLine)> onScrolledToLine;
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
    /// Native host window, used to route keyboard accelerators by focus.
    /// Returns nullptr when the engine has no single host window.
    virtual HWND hwnd() const { return nullptr; }
    /// Destroys the native control and unhooks events. Safe to call twice.
    virtual void Destroy() = 0;
    virtual void SetBounds(const RECT& rc) = 0;
    /// Resizes during an interactive drag. Engines may use a lighter path than
    /// SetBounds() (for example, skipping scroll restoration and background
    /// erase) to keep live splitter dragging smooth. The default delegates.
    virtual void SetLiveBounds(const RECT& rc) { SetBounds(rc); }
    virtual void Show(bool visible) = 0;

    /// Replaces the rendered document.
    ///
    /// Implementations may apply the HTML asynchronously (the IE control is
    /// ready some time after creation); callers must not assume the new HTML
    /// is visible when this returns.
    virtual void SetHtml(const std::string& html, const std::string& baseUrl) = 0;
    virtual void SetZoom(int percent) = 0;
    /// Navigates the preview to a section anchor (for TOC navigation).
    virtual void ScrollToAnchor(const std::string& anchor) = 0;
    /// Scrolls so the given 1-based Markdown source line is at the top.
    virtual void ScrollToSourceLine(double sourceLine) = 0;
    /// Marks cached source positions stale after a DOM update that did not go
    /// through SetHtml (for example an asynchronously inserted diagram image).
    virtual void InvalidateScrollMap() {}
    /// True when the rendered document exposes source line positions.
    virtual bool SupportsSourceMapping() const = 0;
    /// Executes JavaScript. Returns false if the engine does not support it.
    virtual bool RunJavaScript(const std::string& script) = 0;

    /// Executes JavaScript and returns the text of the element with the given
    /// id afterwards (UTF-8), or an empty string when unavailable.
    ///
    /// Used to read small results back from the page without a COM callback.
    virtual std::string RunJavaScriptForResult(const std::string& script,
                                               const std::string& resultElementId)
    {
        (void)script;
        (void)resultElementId;
        return {};
    }

    virtual void SetCallbacks(PreviewCallbacks callbacks) = 0;

    /// Gives keyboard focus to the preview surface.
    virtual void Focus() = 0;

    /// Forwards a keyboard message to the underlying control so it can handle
    /// accelerators (e.g. Ctrl+C to copy). Returns true if the message was
    /// handled and should not be processed further.
    virtual bool TranslateAccelerator(MSG* msg) = 0;

    virtual bool SupportsJavaScript() const = 0;

    /// Invoked when local files are dropped on the preview surface.
    /// `isSupported` filters which paths can be opened (returns true when the
    /// file format is handled). Engines that cannot intercept their native
    /// drop target may ignore this.
    virtual void SetFileDropHandler(
        std::function<void(std::vector<std::wstring>)> /*handler*/,
        std::function<bool(const std::wstring&)> /*isSupported*/)
    {
    }
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
