#include "ie/iepreviewview.h"

#include <mshtml.h>

#include <atlsafe.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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
    UnadviseWindowEvents();
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
    scrollMap_ = ui::ScrollMap{};
    scrollMapDirty_ = true;
    hasSourceMap_ = false;
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
    const bool boundsChanged = rect.left != lastBounds_.left || rect.top != lastBounds_.top ||
                               rect.right != lastBounds_.right ||
                               rect.bottom != lastBounds_.bottom;

    ax_.MoveWindow(&rect, TRUE);
    lastBounds_ = rect;

    if (restoreScroll) {
        RestoreScrollPosition(top, left);
    }

    // Resizing reflows the document, so all cached pixel positions change.
    scrollMapDirty_ = true;

    // Moving the ATL host does not always repaint the inner MSHTML surface
    // (the pane can show the parent background). Force a child-inclusive
    // invalidation; the host applies the new layout once per splitter drag.
    if (boundsChanged) {
        ::RedrawWindow(ax_.m_hWnd, nullptr, nullptr,
                       RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
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
    const HRESULT hr =
        IDispEventSimpleImpl<1, IePreviewView, &DIID_DWebBrowserEvents2>::DispEventAdvise(
            browser_, &DIID_DWebBrowserEvents2);
    advised_ = SUCCEEDED(hr);
    return advised_;
}

void IePreviewView::UnadviseBrowser()
{
    if (advised_ && browser_ != nullptr) {
        IDispEventSimpleImpl<1, IePreviewView, &DIID_DWebBrowserEvents2>::DispEventUnadvise(
            browser_, &DIID_DWebBrowserEvents2);
    }
    advised_ = false;
}

void IePreviewView::AdviseWindowEvents()
{
    UnadviseWindowEvents();

    scrollWindow_ = Window();
    if (scrollWindow_ == nullptr) {
        return;
    }

    const HRESULT hr =
        IDispEventSimpleImpl<2, IePreviewView, &DIID_HTMLWindowEvents2>::DispEventAdvise(
            scrollWindow_, &DIID_HTMLWindowEvents2);
    windowAdvised_ = SUCCEEDED(hr);
}

void IePreviewView::UnadviseWindowEvents()
{
    if (windowAdvised_ && scrollWindow_ != nullptr) {
        IDispEventSimpleImpl<2, IePreviewView, &DIID_HTMLWindowEvents2>::DispEventUnadvise(
            scrollWindow_, &DIID_HTMLWindowEvents2);
    }
    windowAdvised_ = false;
    scrollWindow_.Release();
}

void IePreviewView::ScrollToSourceLine(double sourceLine)
{
    if (!ready_) {
        return;
    }

    EnsureScrollMap();
    if (scrollMap_.empty()) {
        return;
    }

    CComPtr<IHTMLWindow2> window = Window();
    if (window == nullptr) {
        return;
    }

    const double y = std::max(0.0, scrollMap_.yForLine(sourceLine));
    suppressScrollCallback_ = true;
    window->scrollTo(0, static_cast<long>(std::lround(y)));
    suppressScrollCallback_ = false;
}

void IePreviewView::EnsureScrollMap()
{
    if (scrollMapDirty_) {
        RebuildScrollMap();
    }
}

void IePreviewView::RebuildScrollMap()
{
    scrollMapDirty_ = false;
    hasSourceMap_ = false;
    scrollMap_ = ui::ScrollMap{};

    CComPtr<IHTMLWindow2> window = Window();
    if (window == nullptr) {
        return;
    }

    // Collect each block's source line range and its document-space rectangle.
    // The result is written into a hidden element because execScript does not
    // reliably return string values on all IE configurations.
    static const wchar_t* kScript =
        L"(function(){"
        L"var out=[];"
        L"var all=document.getElementsByTagName('*');"
        L"var base=(window.pageYOffset||document.documentElement.scrollTop||0);"
        L"for(var i=0;i<all.length;i++){"
        L"var e=all[i];"
        L"var sp=e.getAttribute?e.getAttribute('data-sourcepos'):null;"
        L"if(!sp) continue;"
        L"var m=/(\\d+):\\d+-(\\d+):\\d+/.exec(sp);"
        L"if(!m) continue;"
        L"var r=e.getBoundingClientRect();"
        L"var h=r.bottom-r.top;"
        L"if(h<=0) continue;"
        L"out.push([+m[1],+m[2],Math.round(r.top+base),Math.round(h)]);"
        L"}"
        L"var h=document.getElementById('awm-scroll-map');"
        L"if(!h){h=document.createElement('div');h.id='awm-scroll-map';"
        L"h.style.display='none';document.body.appendChild(h);}"
        L"h.innerHTML=JSON.stringify(out);"
        L"})()";

    CComVariant result;
    const HRESULT hr = window->execScript(CComBSTR(kScript), CComBSTR(L"JavaScript"), &result);
    if (FAILED(hr)) {
        return;
    }

    CComPtr<IHTMLDocument2> document = Document();
    CComQIPtr<IHTMLDocument3> document3(document);
    if (document3 == nullptr) {
        return;
    }
    CComPtr<IHTMLElement> holder;
    if (FAILED(document3->getElementById(CComBSTR(L"awm-scroll-map"), &holder)) ||
        holder == nullptr) {
        return;
    }
    CComBSTR text;
    if (FAILED(holder->get_innerHTML(&text)) || text.m_str == nullptr) {
        return;
    }

    try {
        const std::string json = wideToUtf8(std::wstring(text.m_str, text.Length()));
        const nlohmann::json rows = nlohmann::json::parse(json);
        if (!rows.is_array()) {
            return;
        }

        std::vector<ui::ScrollBlock> blocks;
        blocks.reserve(rows.size());
        for (const auto& row : rows) {
            if (!row.is_array() || row.size() < 4) {
                continue;
            }
            ui::ScrollBlock block;
            block.startLine = row[0].get<int>();
            block.endLine = row[1].get<int>();
            block.top = row[2].get<double>();
            block.height = row[3].get<double>();
            blocks.push_back(block);
        }

        scrollMap_.rebuild(std::move(blocks));
        hasSourceMap_ = !scrollMap_.empty();
    } catch (const nlohmann::json::exception&) {
        // Malformed map: leave it empty so synchronization is skipped safely.
    }
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
    scrollMapDirty_ = true;
    hasSourceMap_ = false;
    RestoreScrollPosition(scrollTop, scrollLeft);
    InstallLinkHandler();
    AdviseWindowEvents();
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
    AdviseWindowEvents();
    FlushPending();
}

void __stdcall IePreviewView::OnWindowScroll(IHTMLEventObj* /*event*/)
{
    if (suppressScrollCallback_ || !callbacks_.onScrolledToLine) {
        return;
    }

    EnsureScrollMap();
    if (scrollMap_.empty()) {
        return;
    }

    long top = 0;
    long left = 0;
    if (!GetScrollPosition(top, left)) {
        return;
    }

    callbacks_.onScrolledToLine(scrollMap_.lineForY(static_cast<double>(top)));
}

void __stdcall IePreviewView::OnWindowLoad(IHTMLEventObj* /*event*/)
{
    scrollMapDirty_ = true;
    RebuildScrollMap();
}

void __stdcall IePreviewView::OnWindowResize(IHTMLEventObj* /*event*/)
{
    // Rebuild lazily: resize events can arrive rapidly while dragging a
    // splitter, and the pixel positions settle after the current message.
    scrollMapDirty_ = true;
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
