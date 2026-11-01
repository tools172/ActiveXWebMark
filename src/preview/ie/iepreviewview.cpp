#include "ie/iepreviewview.h"

#include <mshtml.h>

#include <atlsafe.h>
#include <utility>

#include "util/stringutil.h"

namespace awm {

IePreviewView::IePreviewView() = default;

IePreviewView::~IePreviewView()
{
    Destroy();
}

bool IePreviewView::Create(HWND host, const RECT& rc)
{
    host_ = host;

    RECT rect = rc;
    ax_.Create(host, rect, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
    if (!ax_.IsWindow()) {
        return false;
    }

    if (FAILED(ax_.CreateControl(L"Shell.Explorer"))) {
        return false;
    }

    if (FAILED(ax_.QueryControl(IID_IWebBrowser2, reinterpret_cast<void**>(&browser_)))) {
        return false;
    }

    // Used to forward keyboard accelerators (e.g. Ctrl+C) to the control.
    ax_.QueryControl(IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&activeObject_));

    return true;
}

void IePreviewView::Destroy()
{
    if (browser_ != nullptr) {
        browser_.Release();
    }
    if (ax_.IsWindow()) {
        ax_.DestroyWindow();
    }
    host_ = nullptr;
}

void IePreviewView::SetBounds(const RECT& rc)
{
    if (ax_.IsWindow()) {
        ax_.MoveWindow(&rc, TRUE);
    }
}

void IePreviewView::Show(bool visible)
{
    if (ax_.IsWindow()) {
        ax_.ShowWindow(visible ? SW_SHOW : SW_HIDE);
    }
}

void IePreviewView::SetHtml(const std::string& html, const std::string& /*baseUrl*/)
{
    if (browser_ == nullptr) {
        return;
    }

    pendingHtml_ = utf8ToWide(html);
    WritePendingHtml();
}

void IePreviewView::WritePendingHtml()
{
    if (pendingHtml_.empty() || browser_ == nullptr) {
        return;
    }

    // Ensure there is a blank document to write into.
    CComVariant url(L"about:blank");
    browser_->Navigate2(&url, nullptr, nullptr, nullptr, nullptr);

    // Pump queued messages and poll until the document reaches "complete".
    for (int attempt = 0; attempt < 200; ++attempt) {
        CComPtr<IDispatch> disp;
        if (SUCCEEDED(browser_->get_Document(&disp)) && disp != nullptr) {
            CComQIPtr<IHTMLDocument2> doc(disp);
            if (doc != nullptr) {
                BSTR state = nullptr;
                const bool complete = SUCCEEDED(doc->get_readyState(&state)) &&
                                      state != nullptr && wcscmp(state, L"complete") == 0;
                if (state != nullptr) {
                    ::SysFreeString(state);
                }
                if (complete) {
                    CComSafeArray<VARIANT> sa(1);
                    sa[0] = CComVariant(pendingHtml_.c_str());
                    doc->write(sa);
                    doc->close();
                    pendingHtml_.clear();
                    return;
                }
            }
        }

        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        ::Sleep(10);
    }
    // Leave pendingHtml_ set; a later write attempt will retry.
}

void IePreviewView::SetZoom(int percent)
{
    if (browser_ == nullptr) {
        return;
    }
    CComVariant zoom(static_cast<long>(percent));
    browser_->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, &zoom, nullptr);
}

void IePreviewView::ScrollToAnchor(const std::string& anchor)
{
    // Scroll the element into view directly, instead of assigning location.hash
    // (which IE rejects as an invalid URL on an about:blank document).
    const std::string js =
        "var e=document.getElementById('" + anchor + "');if(e)e.scrollIntoView();";
    RunJavaScript(js);
}

bool IePreviewView::RunJavaScript(const std::string& script)
{
    if (browser_ == nullptr) {
        return false;
    }

    CComPtr<IDispatch> disp;
    if (FAILED(browser_->get_Document(&disp)) || disp == nullptr) {
        return false;
    }

    CComQIPtr<IHTMLDocument2> doc(disp);
    if (doc == nullptr) {
        return false;
    }

    CComPtr<IHTMLWindow2> window;
    if (FAILED(doc->get_parentWindow(&window)) || window == nullptr) {
        return false;
    }

    CComBSTR code(script.c_str());
    CComVariant result;
    return SUCCEEDED(window->execScript(code, CComBSTR(L"JavaScript"), &result));
}

void IePreviewView::SetCallbacks(PreviewCallbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

bool IePreviewView::TranslateAccelerator(MSG* msg)
{
    if (activeObject_ == nullptr) {
        return false;
    }
    return activeObject_->TranslateAccelerator(msg) == S_OK;
}

} // namespace awm
