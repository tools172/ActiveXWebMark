#pragma once

#include <Windows.h>

#include <functional>
#include <string>
#include <vector>

#include "highlight/isyntaxhighlighter.h"
#include "theme/theme_catalog.h"

namespace awm {

/// Engine-agnostic Markdown source editor.
///
/// The first implementation wraps Scintilla; the interface keeps the rest of
/// the application independent of that choice.
class IEditorView {
public:
    virtual ~IEditorView() = default;

    virtual bool Create(HWND parent, const RECT& rc) = 0;

    /// Native window of the editor (used to route parent notifications).
    virtual HWND hwnd() const = 0;

    /// Handles a WM_NOTIFY forwarded by the editor's parent window.
    ///
    /// Scintilla sends SCN_MODIFIED / SCN_UPDATEUI to its parent, not to its
    /// own window procedure, so the host container must forward them here.
    virtual void HandleHostNotification(LPNMHDR header) = 0;

    virtual void SetBounds(const RECT& rc) = 0;
    virtual void Show(bool visible) = 0;

    virtual void SetText(const std::string& utf8) = 0;
    virtual std::string GetText() const = 0;

    /// Applies semantic highlighting spans (byte offsets into `source`).
    ///
    /// Passing the source avoids a second full-buffer round trip through the
    /// editor after every render.
    virtual void ApplyHighlighting(const std::string& source,
                                   const std::vector<StyleSpan>& spans) = 0;

    /// Moves the caret to a 1-based line (used by TOC navigation).
    virtual void GotoLine(int line) = 0;

    /// 1-based source line at the top of the visible area.
    virtual int FirstVisibleLine() const = 0;

    /// Scrolls so `line` (1-based) is the topmost visible source line.
    virtual void SetFirstVisibleLine(int line) = 0;

    /// Invoked when the vertical scroll position changes.
    virtual void SetScrollCallback(std::function<void(int firstVisibleLine)> callback) = 0;

    virtual void SetReadOnly(bool readOnly) = 0;

    /// Updates DPI-dependent fonts/margins (1.0 = 96 DPI).
    virtual void SetDpiScale(double scale) = 0;

    /// Applies the colors of the active application theme. Implementations
    /// that cannot be themed may ignore this.
    virtual void SetColorScheme(const theme::EditorScheme& /*scheme*/) {}

    /// Gives keyboard focus to the text area.
    virtual void Focus() = 0;

    /// Invoked when the user edits the document.
    virtual void SetChangeCallback(std::function<void()> callback) = 0;

    /// Invoked when local files are dropped on the editor surface.
    /// `isSupported` filters which paths can be opened (returns true when the
    /// file format is handled). Engines that cannot intercept their native
    /// drop target may ignore this.
    virtual void SetFileDropHandler(
        std::function<void(std::vector<std::wstring>)> /*handler*/,
        std::function<bool(const std::wstring&)> /*isSupported*/)
    {
    }
};

} // namespace awm
