#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ieditorview.h"
#include "highlight/isyntaxhighlighter.h"

namespace awm {

namespace ui {
class FileDropTarget;
}

/// IEditorView backed by the Scintilla editing component.
///
/// Markdown source is kept as UTF-8; syntax highlighting is host-driven via
/// Scintilla's container lexer (SCLEX_CONTAINER) from PMH spans.
class ScintillaEditor : public IEditorView {
public:
    ScintillaEditor();
    ~ScintillaEditor() override;

    bool Create(HWND parent, const RECT& rc) override;
    HWND hwnd() const override { return hwnd_; }
    void HandleHostNotification(LPNMHDR header) override;
    void SetBounds(const RECT& rc) override;
    void Show(bool visible) override;
    void SetText(const std::string& utf8) override;
    std::string GetText() const override;
    void ApplyHighlighting(const std::string& source,
                           const std::vector<StyleSpan>& spans) override;
    void GotoLine(int line) override;
    int FirstVisibleLine() const override;
    void SetFirstVisibleLine(int line) override;
    void SetScrollCallback(std::function<void(int firstVisibleLine)> callback) override;
    void SetReadOnly(bool readOnly) override;
    void SetDpiScale(double scale) override;
    void SetColorScheme(const theme::EditorScheme& scheme) override;
    void Focus() override;
    void SetChangeCallback(std::function<void()> callback) override;
    void SetFileDropHandler(
        std::function<void(std::vector<std::wstring>)> handler,
        std::function<bool(const std::wstring&)> isSupported) override;

private:
    void SetupStyles();
    void InstallFileDropTarget();
    void ReleaseFileDropTarget();
    void UpdateMargins();
    void UpdateLineNumberWidth();
    void NotifyScrollIfChanged();
    void ApplyStyle(const std::string& source, const std::vector<StyleSpan>& spans);
    int px(int logicalPixels) const;

    static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    WNDPROC originalProc_ = nullptr;
    ui::FileDropTarget* fileDropTarget_ = nullptr;
    std::function<void(std::vector<std::wstring>)> fileDropHandler_;
    std::function<bool(const std::wstring&)> fileDropFilter_;
    std::function<void()> changeCallback_;
    std::function<void(int)> scrollCallback_;
    double dpiScale_ = 1.0;
    int lineNumberWidth_ = -1;
    int lastReportedFirstLine_ = 1;
    theme::EditorScheme scheme_;
};

} // namespace awm
