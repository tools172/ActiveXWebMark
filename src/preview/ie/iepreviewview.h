#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <atlhost.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>

#include <string>

#include "ipreviewview.h"

namespace awm {

namespace ie_detail {

// ATL needs explicit dispatch signatures for IDispEventSimpleImpl.
inline _ATL_FUNC_INFO kDocumentCompleteInfo = {
    CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, static_cast<VARTYPE>(VT_BYREF | VT_VARIANT)}};

inline _ATL_FUNC_INFO kBeforeNavigate2Info = {
    CC_STDCALL,
    VT_EMPTY,
    7,
    {VT_DISPATCH, static_cast<VARTYPE>(VT_BYREF | VT_VARIANT),
     static_cast<VARTYPE>(VT_BYREF | VT_VARIANT), static_cast<VARTYPE>(VT_BYREF | VT_VARIANT),
     static_cast<VARTYPE>(VT_BYREF | VT_VARIANT), static_cast<VARTYPE>(VT_BYREF | VT_VARIANT),
     static_cast<VARTYPE>(VT_BYREF | VT_BOOL)}};

inline _ATL_FUNC_INFO kNewWindow3Info = {
    CC_STDCALL,
    VT_EMPTY,
    5,
    {static_cast<VARTYPE>(VT_BYREF | VT_DISPATCH), static_cast<VARTYPE>(VT_BYREF | VT_BOOL),
     VT_UI4, VT_BSTR, VT_BSTR}};

} // namespace ie_detail

/// IPreviewView backed by the IE WebBrowser control (MSHTML / Trident).
///
/// The control is single-process and ships with Windows. HTML is written
/// asynchronously after the initial about:blank document becomes ready, so
/// rendering never pumps messages or blocks the UI thread. External links are
/// cancelled and reported through PreviewCallbacks::onLink instead of
/// navigating the preview away from the rendered document.
class IePreviewView final
    : public IPreviewView
    , public IDispEventSimpleImpl<1, IePreviewView, &DIID_DWebBrowserEvents2>
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
    void Focus() override;

    BEGIN_SINK_MAP(IePreviewView)
        SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, OnDocumentComplete,
                        &ie_detail::kDocumentCompleteInfo)
        SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2, OnBeforeNavigate2,
                        &ie_detail::kBeforeNavigate2Info)
        SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3, OnNewWindow3,
                        &ie_detail::kNewWindow3Info)
    END_SINK_MAP()

    void __stdcall OnDocumentComplete(IDispatch* dispatch, VARIANT* url);
    void __stdcall OnBeforeNavigate2(IDispatch* dispatch, VARIANT* url, VARIANT* flags,
                                     VARIANT* targetFrameName, VARIANT* postData,
                                     VARIANT* headers, VARIANT_BOOL* cancel);
    void __stdcall OnNewWindow3(IDispatch** dispatch, VARIANT_BOOL* cancel, DWORD flags,
                                BSTR urlContext, BSTR url);

private:
    bool AdviseBrowser();
    void UnadviseBrowser();
    void NavigateToBlank();
    void FlushPending();
    void WriteHtml(const std::wstring& html);
    void InstallLinkHandler();
    void ApplyPendingAnchor();
    bool GetScrollPosition(long& top, long& left) const;
    void RestoreScrollPosition(long top, long left);

    CComPtr<IHTMLDocument2> Document() const;
    CComPtr<IHTMLWindow2> Window() const;

    HWND host_ = nullptr;
    CAxWindow ax_;
    CComPtr<IWebBrowser2> browser_;
    CComPtr<IOleInPlaceActiveObject> activeObject_;
    PreviewCallbacks callbacks_;

    std::wstring pendingHtml_;
    std::wstring lastHtml_;
    std::string pendingAnchor_;
    bool ready_ = false;
    bool navigating_ = false;
    bool advised_ = false;
};

} // namespace awm
