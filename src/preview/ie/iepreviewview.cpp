#include "ie/iepreviewview.h"

#include <mshtml.h>

#include <atlsafe.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <utility>

#include "util/stringutil.h"

namespace awm {

namespace {

bool startsWithNoCase(const std::wstring& value, const wchar_t* prefix)
{
    const std::size_t length = std::wcslen(prefix);
    return value.size() >= length && ::_wcsnicmp(value.c_str(), prefix, length) == 0;
}

// The rendered document lives at about:blank; fragment links resolve against
// that URL, so they must be allowed to stay inside the preview.
bool isInternalUrl(const std::wstring& url)
{
    return startsWithNoCase(url, L"about:blank");
}

bool isSafeExternalUrl(const std::wstring& url)
{
    return startsWithNoCase(url, L"http:") || startsWithNoCase(url, L"https:") ||
           startsWithNoCase(url, L"file:") || startsWithNoCase(url, L"mailto:");
}

std::string escapeForJavaScript(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        if (ch == '\\' || ch == '\'') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

} // namespace

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

    // Never surface MSHTML script-error dialogs from the preview.
    browser_->put_Silent(VARIANT_TRUE);

    // Used to forward keyboard accelerators (e.g. Ctrl+C) to the control.
    ax_.QueryControl(IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&activeObject_));

    AdviseBrowser();
    NavigateToBlank();
    return true;
}

void IePreviewView::Destroy()
{
    UnadviseBrowser();
    if (activeObject_ != nullptr) {
        activeObject_.Release();
    }
    if (browser_ != nullptr) {
        browser_.Release();
    }
    if (ax_.IsWindow()) {
        ax_.DestroyWindow();
    }
    host_ = nullptr;
    ready_ = false;
    navigating_ = false;
    pendingHtml_.clear();
    lastHtml_.clear();
    pendingAnchor_.clear();
}

void IePreviewView::SetBounds(const RECT& rc)
{
    if (!ax_.IsWindow()) {
        return;
    }

    // Keep the reader's position when a splitter drag resizes the preview.
    long top = 0;
    long left = 0;
    const bool restoreScroll = ready_ && GetScrollPosition(top, left);

    RECT rect = rc;
    ax_.MoveWindow(&rect, TRUE);

    if (restoreScroll) {
        RestoreScrollPosition(top, left);
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
    pendingHtml_ = utf8ToWide(html);

    // Fallback in case the DocumentComplete event was missed (for example when
    // a different document container was active at creation time).
    if (!ready_) {
        CComPtr<IHTMLDocument2> document = Document();
        if (document != nullptr) {
            BSTR state = nullptr;
            if (SUCCEEDED(document->get_readyState(&state)) && state != nullptr) {
                ready_ = std::wcscmp(state, L"complete") == 0;
                ::SysFreeString(state);
            }
        }
    }

    if (ready_) {
        FlushPending();
    } else if (!navigating_) {
        NavigateToBlank();
    }
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
    pendingAnchor_ = anchor;
    if (ready_) {
        ApplyPendingAnchor();
    }
}

bool IePreviewView::RunJavaScript(const std::string& script)
{
    CComPtr<IHTMLWindow2> window = Window();
    if (window == nullptr) {
        return false;
    }

    // Scripts are UTF-8 (they may contain Chinese heading anchors). Convert to
    // UTF-16 explicitly: CComBSTR(const char*) uses the ANSI code page and can
    // swallow a following quote when a UTF-8 byte pair lands on a DBCS lead.
    const std::wstring wideScript = utf8ToWide(script);
    CComBSTR code(wideScript.c_str());
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

void IePreviewView::Focus()
{
    if (ax_.IsWindow()) {
        ::SetFocus(ax_.m_hWnd);
    }
}

bool IePreviewView::AdviseBrowser()
{
    if (browser_ == nullptr || advised_) {
        return browser_ != nullptr;
    }
    advised_ = SUCCEEDED(DispEventAdvise(browser_, &DIID_DWebBrowserEvents2));
    return advised_;
}

void IePreviewView::UnadviseBrowser()
{
    if (advised_ && browser_ != nullptr) {
        DispEventUnadvise(browser_, &DIID_DWebBrowserEvents2);
    }
    advised_ = false;
}

void IePreviewView::NavigateToBlank()
{
    if (browser_ == nullptr) {
        return;
    }
    CComVariant url(L"about:blank");
    navigating_ = true;
    browser_->Navigate2(&url, nullptr, nullptr, nullptr, nullptr);
}

void IePreviewView::FlushPending()
{
    if (!ready_) {
        return;
    }

    if (pendingHtml_ != lastHtml_) {
        std::wstring html = std::move(pendingHtml_);
        pendingHtml_.clear();
        WriteHtml(html);
    } else {
        pendingHtml_.clear();
    }
    ApplyPendingAnchor();
}

void IePreviewView::WriteHtml(const std::wstring& html)
{
    CComPtr<IHTMLDocument2> document = Document();
    if (document == nullptr) {
        return;
    }

    long scrollTop = 0;
    long scrollLeft = 0;
    GetScrollPosition(scrollTop, scrollLeft);

    CComSafeArray<VARIANT> array(1);
    array[0] = CComVariant(html.c_str());
    document->write(array);
    document->close();

    lastHtml_ = html;
    RestoreScrollPosition(scrollTop, scrollLeft);
    InstallLinkHandler();
}

void IePreviewView::InstallLinkHandler()
{
    // The preview uses <base href="file:///..."> so relative images resolve.
    // That also makes in-page "#anchor" links resolve to the base document
    // URL; intercept them here so they scroll instead of navigating away.
    static const char* kScript =
        "document.onclick=function(ev){"
        "if(!ev){ev=window.event;}"
        "var t=(ev&&(ev.target||ev.srcElement))||null;"
        "while(t&&(!t.tagName||t.tagName.toUpperCase()!='A')){t=t.parentNode;}"
        "if(!t){return;}"
        "var h=t.getAttribute('href')||'';"
        "if(h.charAt(0)!='#'){return;}"
        "var e=document.getElementById(h.substring(1));"
        "if(e){var r=e.getBoundingClientRect();"
        "var d=document.documentElement;var b=document.body;"
        "var y=r.top+(d.scrollTop||b.scrollTop||0)-16;"
        "window.scrollTo(0,y<0?0:y);}"
        "ev.returnValue=false;"
        "if(ev.preventDefault){ev.preventDefault();}"
        "if(ev.stopPropagation){ev.stopPropagation();}"
        "};";
    RunJavaScript(kScript);
}

void IePreviewView::ApplyPendingAnchor()
{
    if (pendingAnchor_.empty() || !ready_) {
        return;
    }

    const std::string anchor = std::move(pendingAnchor_);
    pendingAnchor_.clear();

    // scrollIntoView() is a no-op in IE when the element is already fully
    // visible; compute the absolute position and scroll explicitly instead.
    const std::string script =
        "var e=document.getElementById('" + escapeForJavaScript(anchor) +
        "');if(e){var r=e.getBoundingClientRect();var d=document.documentElement;"
        "var b=document.body;var y=r.top+(d.scrollTop||b.scrollTop||0)-16;"
        "window.scrollTo(0,y<0?0:y);}";
    RunJavaScript(script);
}

bool IePreviewView::GetScrollPosition(long& top, long& left) const
{
    if (browser_ == nullptr) {
        return false;
    }

    CComPtr<IDispatch> dispatch;
    if (FAILED(browser_->get_Document(&dispatch)) || dispatch == nullptr) {
        return false;
    }

    CComQIPtr<IHTMLDocument3> document(dispatch);
    if (document == nullptr) {
        return false;
    }

    CComPtr<IHTMLElement> element;
    if (FAILED(document->get_documentElement(&element)) || element == nullptr) {
        return false;
    }

    CComQIPtr<IHTMLElement2> scrollable(element);
    if (scrollable == nullptr) {
        return false;
    }
    return SUCCEEDED(scrollable->get_scrollTop(&top)) &&
           SUCCEEDED(scrollable->get_scrollLeft(&left));
}

void IePreviewView::RestoreScrollPosition(long top, long left)
{
    CComPtr<IHTMLWindow2> window = Window();
    if (window != nullptr) {
        window->scrollTo(left, top);
    }
}

CComPtr<IHTMLDocument2> IePreviewView::Document() const
{
    if (browser_ == nullptr) {
        return nullptr;
    }
    CComPtr<IDispatch> dispatch;
    if (FAILED(browser_->get_Document(&dispatch)) || dispatch == nullptr) {
        return nullptr;
    }
    return CComQIPtr<IHTMLDocument2>(dispatch);
}

CComPtr<IHTMLWindow2> IePreviewView::Window() const
{
    CComPtr<IHTMLDocument2> document = Document();
    if (document == nullptr) {
        return nullptr;
    }
    CComPtr<IHTMLWindow2> window;
    document->get_parentWindow(&window);
    return window;
}

void __stdcall IePreviewView::OnDocumentComplete(IDispatch* /*dispatch*/, VARIANT* /*url*/)
{
    ready_ = true;
    navigating_ = false;
    FlushPending();
}

void __stdcall IePreviewView::OnBeforeNavigate2(IDispatch* /*dispatch*/, VARIANT* url,
                                                VARIANT* /*flags*/, VARIANT* /*targetFrameName*/,
                                                VARIANT* /*postData*/, VARIANT* /*headers*/,
                                                VARIANT_BOOL* cancel)
{
    if (cancel != nullptr) {
        *cancel = VARIANT_FALSE;
    }
    if (url == nullptr || url->vt != VT_BSTR || url->bstrVal == nullptr) {
        return;
    }

    const std::wstring target(url->bstrVal, ::SysStringLen(url->bstrVal));
    if (target.empty() || isInternalUrl(target)) {
        return;
    }

    // Keep the preview on the rendered document; hand real links to the host.
    if (cancel != nullptr) {
        *cancel = VARIANT_TRUE;
    }
    if (isSafeExternalUrl(target) && callbacks_.onLink) {
        callbacks_.onLink(wideToUtf8(target));
    }
}

void __stdcall IePreviewView::OnNewWindow3(IDispatch** /*dispatch*/, VARIANT_BOOL* cancel,
                                           DWORD /*flags*/, BSTR /*urlContext*/, BSTR url)
{
    if (cancel != nullptr) {
        *cancel = VARIANT_TRUE;
    }
    if (url == nullptr) {
        return;
    }

    const std::wstring target(url, ::SysStringLen(url));
    if (isSafeExternalUrl(target) && callbacks_.onLink) {
        callbacks_.onLink(wideToUtf8(target));
    }
}

} // namespace awm
