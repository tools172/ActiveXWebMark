#pragma once

#include <Windows.h>

#include <functional>
#include <string>
#include <vector>

#include "highlight/isyntaxhighlighter.h"

namespace awm {

/// Engine-agnostic Markdown source editor.
///
/// The first implementation wraps Scintilla; the interface keeps the rest of
/// the application independent of that choice.
class IEditorView {
public:
    virtual ~IEditorView() = default;

    virtual bool Create(HWND parent, const RECT& rc) = 0;

    virtual void SetBounds(const RECT& rc) = 0;
    virtual void Show(bool visible) = 0;

    virtual void SetText(const std::string& utf8) = 0;
    virtual std::string GetText() const = 0;

    /// Applies semantic highlighting spans (byte offsets into the source).
    virtual void ApplyHighlighting(const std::vector<StyleSpan>& spans) = 0;

    /// Moves the caret to a 1-based line (used by TOC navigation).
    virtual void GotoLine(int line) = 0;

    virtual void SetReadOnly(bool readOnly) = 0;

    /// Invoked when the user edits the document.
    virtual void SetChangeCallback(std::function<void()> callback) = 0;
};

} // namespace awm
