#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ieditorview.h"
#include "highlight/isyntaxhighlighter.h"

namespace awm {

/// IEditorView backed by the Scintilla editing component.
///
/// Markdown source is kept as UTF-8; syntax highlighting is host-driven via
/// Scintilla's container lexer (SCLEX_CONTAINER) from PMH spans.
class ScintillaEditor : public IEditorView {
public:
    ScintillaEditor();
    ~ScintillaEditor() override;

    bool Create(HWND parent, const RECT& rc) override;
    void SetBounds(const RECT& rc) override;
    void Show(bool visible) override;
    void SetText(const std::string& utf8) override;
    std::string GetText() const override;
    void ApplyHighlighting(const std::vector<StyleSpan>& spans) override;
    void GotoLine(int line) override;
    void SetReadOnly(bool readOnly) override;
    void SetChangeCallback(std::function<void()> callback) override;

private:
    void SetupStyles();
    void ApplyStyle(const std::string& source, const std::vector<StyleSpan>& spans);

    static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    WNDPROC originalProc_ = nullptr;
    std::function<void()> changeCallback_;
};

} // namespace awm
