#pragma once

#include <atlbase.h>
#include <atlhost.h>
#include <exdisp.h>

#include <string>

#include "ipreviewview.h"

namespace awm {

/// IPreviewView backed by the IE WebBrowser control (MSHTML / Trident).
///
/// Single-process, ships with Windows, and sufficient for rendering the
/// static HTML produced by the Markdown engine.
class IePreviewView final : public IPreviewView
{
public:
    IePreviewView();
    ~IePreviewView() override;

    bool Create(HWND host, const RECT& rc) override;
    void Destroy() override;
    void SetBounds(const RECT& rc) override;
    void Show(bool visible) override;

    void SetHtml(const std::string& html, const std::string& baseUrl) override;
    void SetZoom(int percent) override;
    void ScrollToAnchor(const std::string& anchor) override;
    bool RunJavaScript(const std::string& script) override;

    void SetCallbacks(PreviewCallbacks callbacks) override;
    bool TranslateAccelerator(MSG* msg) override;
    bool SupportsJavaScript() const override { return true; }

private:
    void WritePendingHtml();

    HWND host_ = nullptr;
    CAxWindow ax_;
    CComPtr<IWebBrowser2> browser_;
    CComPtr<IOleInPlaceActiveObject> activeObject_;
    PreviewCallbacks callbacks_;
    std::wstring pendingHtml_;
};

} // namespace awm
