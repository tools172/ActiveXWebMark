#include "documentview.h"

#include <algorithm>

#include <cmath>
#include <cstdio>
#include <sstream>
#include <utility>

#include "darkmode.h"
#include "theme/theme_catalog.h"
#include "themeapply.h"

#include "markdown/previewhtml.h"
#include "scrollsync.h"
#include "util/pathutil.h"
#include "util/stringutil.h"

namespace awm {

namespace {

RECT toRect(const ui::RectI& rect)
{
    return RECT{rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
}

bool outlineEquals(const Outline& left, const Outline& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].level != right[i].level || left[i].text != right[i].text ||
            left[i].line != right[i].line || left[i].anchor != right[i].anchor) {
            return false;
        }
    }
    return true;
}

std::vector<ui::NavPanel::Item> toNavItems(const Outline& outline)
{
    std::vector<ui::NavPanel::Item> items;
    items.reserve(outline.size());
    for (const OutlineItem& item : outline) {
        items.push_back(ui::NavPanel::Item{utf8ToWide(item.text), item.level});
    }
    return items;
}

// The preview find UI lives in the page: matches are wrapped in styled <span>
// nodes and the current one is scrolled into view. The script writes a small
// status string into a hidden element, which the host reads back through
// IPreviewView::RunJavaScriptForResult().
const char* kFindScript = R"JS(
(function(){
function res(s){
var e=document.getElementById('awm-find-result');
if(!e){
e=document.createElement('div');e.id='awm-find-result';e.style.display='none';
document.body.appendChild(e);
}
e.innerHTML=s;
}
function clearMarks(){
var all=document.getElementsByTagName('span'),m=[];
for(var i=0;i<all.length;i++){
if((' '+all[i].className+' ').indexOf(' awm-find ')>-1){m.push(all[i]);}
}
for(var j=m.length-1;j>=0;j--){
var x=m[j],p=x.parentNode;
if(!p){continue;}
while(x.firstChild){p.insertBefore(x.firstChild,x);}
p.removeChild(x);
if(p.normalize){p.normalize();}
}
}
function select(i){
var st=window.__awmFind;
if(!st||!st.marks.length){res('ok 0 -1');return;}
if(i<0){i=st.marks.length-1;}
if(i>=st.marks.length){i=0;}
st.index=i;
for(var k=0;k<st.marks.length;k++){
var mk=st.marks[k];
mk.className=(k===i)?'awm-find awm-find-current':'awm-find';
}
var r=st.marks[i].getBoundingClientRect();
var d=document.documentElement,b=document.body;
var y=r.top+((d&&d.scrollTop)||(b&&b.scrollTop)||0)-80;
window.scrollTo(0,y<0?0:y);
res('ok '+st.marks.length+' '+i);
}
clearMarks();
var q='__QUERY__';
if(!q){window.__awmFind=null;res('ok 0 -1');return;}
var pattern;
try{
pattern=__REGEX__?q:q.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
if(__WHOLE__){pattern='\\b(?:'+pattern+')\\b';}
var re=new RegExp(pattern,__MATCHCASE__?'g':'gi');
}catch(e){window.__awmFind=null;res('err');return;}
var nodes=[];
(function collect(n){
for(var c=n.firstChild;c;c=c.nextSibling){
if(c.nodeType===3){
var p=c.parentNode;
if(p&&p.id==='awm-find-result'){continue;}
var pt=p&&p.nodeName?p.nodeName.toUpperCase():'';
if(pt!=='SCRIPT'&&pt!=='STYLE'&&pt!=='NOSCRIPT'&&pt!=='TEXTAREA'){nodes.push(c);}
}else if(c.nodeType===1){
var tn=c.nodeName?c.nodeName.toUpperCase():'';
if(tn!=='SCRIPT'&&tn!=='STYLE'&&tn!=='NOSCRIPT'&&tn!=='TEXTAREA'){collect(c);}
}
}
})(document.body);
var total=0;
for(var i=0;i<nodes.length&&total<5000;i++){
var node=nodes[i],text=node.nodeValue||'';
if(!text){continue;}
re.lastIndex=0;
var m,found=[];
while((m=re.exec(text))){
if(m[0].length===0){re.lastIndex++;if(re.lastIndex>text.length){break;}continue;}
found.push([m.index,m.index+m[0].length]);
if(found.length>=5000){break;}
}
if(!found.length){continue;}
var frag=document.createDocumentFragment(),pos=0;
for(var j=0;j<found.length;j++){
var a=found[j][0],b=found[j][1];
if(a>pos){frag.appendChild(document.createTextNode(text.substring(pos,a)));}
var mk=document.createElement('span');
mk.className='awm-find';
frag.appendChild(mk);
mk.appendChild(document.createTextNode(text.substring(a,b)));
pos=b;
total++;
}
if(pos<text.length){frag.appendChild(document.createTextNode(text.substring(pos)));}
var parent=node.parentNode;
if(parent){parent.replaceChild(frag,node);}
}
var all=document.getElementsByTagName('span'),marks=[];
for(var k=0;k<all.length;k++){
if((' '+all[k].className+' ').indexOf(' awm-find ')>-1){marks.push(all[k]);}
}
window.__awmFind={marks:marks,index:-1};
select(0);
})();
)JS";

const char* kFindStepScript = R"JS(
(function(){
function res(s){
var e=document.getElementById('awm-find-result');
if(!e){
e=document.createElement('div');e.id='awm-find-result';e.style.display='none';
document.body.appendChild(e);
}
e.innerHTML=s;
}
var st=window.__awmFind;
if(!st||!st.marks||!st.marks.length){res('ok 0 -1');return;}
var i=st.index+__DELTA__;
if(i<0){i=st.marks.length-1;}
if(i>=st.marks.length){i=0;}
st.index=i;
for(var k=0;k<st.marks.length;k++){
var mk=st.marks[k];
mk.className=(k===i)?'awm-find awm-find-current':'awm-find';
}
var r=st.marks[i].getBoundingClientRect();
var d=document.documentElement,b=document.body;
var y=r.top+((d&&d.scrollTop)||(b&&b.scrollTop)||0)-80;
window.scrollTo(0,y<0?0:y);
res('ok '+st.marks.length+' '+i);
})();
)JS";

const char* kFindClearScript = R"JS(
(function(){
var all=document.getElementsByTagName('span'),m=[];
for(var i=0;i<all.length;i++){
if((' '+all[i].className+' ').indexOf(' awm-find ')>-1){m.push(all[i]);}
}
for(var j=m.length-1;j>=0;j--){
var x=m[j],p=x.parentNode;
if(!p){continue;}
while(x.firstChild){p.insertBefore(x.firstChild,x);}
p.removeChild(x);
if(p.normalize){p.normalize();}
}
window.__awmFind=null;
})();
)JS";

void replaceToken(std::string& text, const std::string& token, const std::string& value)
{
    const std::size_t pos = text.find(token);
    if (pos != std::string::npos) {
        text.replace(pos, token.size(), value);
    }
}

std::string escapeForScript(const std::wstring& value)
{
    const std::string utf8 = wideToUtf8(value);
    std::string escaped;
    escaped.reserve(utf8.size() + 8);
    for (const char ch : utf8) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '\'': escaped += "\\'"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        default:   escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string buildFindScript(const std::wstring& query, const ui::FindBar::Options& options)
{
    std::string script = kFindScript;
    replaceToken(script, "__QUERY__", escapeForScript(query));
    replaceToken(script, "__REGEX__", options.regex ? "true" : "false");
    replaceToken(script, "__WHOLE__", options.wholeWord ? "true" : "false");
    replaceToken(script, "__MATCHCASE__", options.matchCase ? "true" : "false");
    return script;
}

} // namespace

DocumentView::~DocumentView()
{
    if (::IsWindow(m_hWnd)) {
        ::KillTimer(m_hWnd, kRenderTimerId);
        ::KillTimer(m_hWnd, kScrollSyncTimerId);
    }
    if (scrollSync_) {
        scrollSync_->SetPreview(nullptr);
        scrollSync_.reset();
    }
    if (preview_) {
        preview_->Destroy();
        preview_.reset();
    }
    editor_.reset();
    if (::IsWindow(m_hWnd)) {
        ::DestroyWindow(m_hWnd);
    }
}

bool DocumentView::Create(HWND parent, const RECT& rc, std::unique_ptr<DocumentSession> session,
                          const DocumentViewServices& services)
{
    if (!session || services.engine == nullptr || services.highlighter == nullptr ||
        services.theme == nullptr || !services.createEditor) {
        return false;
    }

    session_ = std::move(session);
    services_ = services;
    reportedDirty_ = session_->isDirty();

    RECT rect = rc;
    if (CWindowImpl<DocumentView>::Create(parent, rect, nullptr, WS_CHILD | WS_CLIPCHILDREN,
                                          0) == nullptr) {
        return false;
    }

    RECT zero = {0, 0, 0, 0};

    editor_ = services_.createEditor();
    if (!editor_ || !editor_->Create(m_hWnd, zero)) {
        return false;
    }
    editor_->SetDpiScale(static_cast<double>(theme()->dpi()) / 96.0);

    // Load text and highlight before installing the change callback so the
    // initial content cannot be reported as a user edit.
    const std::string& source = session_->source();
    editor_->SetText(source);
    editor_->ApplyHighlighting(source, services_.highlighter->highlight(source));
    editor_->SetChangeCallback([this]() { OnEditorChanged(); });

    scrollSync_ = std::make_unique<ScrollSyncController>(*editor_);
    scrollSync_->SetScheduler(
        [this] { ::SetTimer(m_hWnd, kScrollSyncTimerId, kScrollSyncDelayMs, nullptr); },
        [this] { ::KillTimer(m_hWnd, kScrollSyncTimerId); });
    scrollSync_->SetEnabled(settings_.scrollSync);
    editor_->SetScrollCallback([this](int line) {
        if (scrollSync_) {
            scrollSync_->OnEditorScrolled(line);
        }
    });

    if (navPanel_.Create(m_hWnd, zero, 100)) {
        navPanel_.SetTheme(theme());
        navPanel_.SetCallbacks(ui::NavPanel::Callbacks{
            [this](std::size_t index) { OnNavSelected(index); }});
        currentOutline_ = session_->outline();
        navPanel_.SetItems(toNavItems(currentOutline_));
    }

    navSplitter_.Create(m_hWnd, zero, 101, ui::SplitterAxis::Vertical);
    navSplitter_.SetTheme(theme());
    navSplitter_.SetCallbacks(ui::Splitter::Callbacks{
        [this](int position) { PreviewSplitterDrag(false, position); },
        [this](int position) { CommitSplitterDrag(false, position); },
        [this]() { HideDragLine(); }});

    editorSplitter_.Create(m_hWnd, zero, 102, ui::SplitterAxis::Vertical);
    editorSplitter_.SetTheme(theme());
    editorSplitter_.SetCallbacks(ui::Splitter::Callbacks{
        [this](int position) { PreviewSplitterDrag(true, position); },
        [this](int position) { CommitSplitterDrag(true, position); },
        [this]() { HideDragLine(); }});

    dragLine_.Create(m_hWnd);
    dragLine_.SetTheme(theme());

    if (findBar_.Create(m_hWnd, zero, 103)) {
        findBar_.SetTheme(theme());
        findBar_.SetCallbacks(ui::FindBar::Callbacks{
            [this](const std::wstring& query, const ui::FindBar::Options& options) {
                OnFindQueryChanged(query, options);
            },
            [this]() { FindNext(); },
            [this]() { FindPrevious(); },
            [this]() { HideFindBar(); }});
        findBar_.ShowWindow(SW_HIDE);
    }

    settings_ = ViewSettings{};
    Layout();
    return true;
}

void DocumentView::SetCallbacks(DocumentViewCallbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

void DocumentView::SetViewSettings(const ViewSettings& settings)
{
    settings_ = settings;
    if (editor_ != nullptr && theme() != nullptr) {
        editor_->SetDpiScale(static_cast<double>(theme()->dpi()) / 96.0);
    }
    Layout();

    if (scrollSync_) {
        scrollSync_->SetEnabled(settings_.scrollSync);
    }
}

void DocumentView::ApplyTheme()
{
    if (theme() == nullptr) {
        return;
    }

    // Controls read palette() while painting, but NavPanel paints its native
    // tree in SetTheme and FindBar caches a background brush, so both must be
    // told again after the palette changed.
    navPanel_.SetTheme(theme());
    navSplitter_.SetTheme(theme());
    editorSplitter_.SetTheme(theme());
    dragLine_.SetTheme(theme());
    if (findBar_.IsWindow()) {
        findBar_.SetTheme(theme());
    }

    const theme::ThemeDefinition& definition = theme::ResolveTheme(settings_.theme);
    const ColorMode colorMode = definition.dark ? ColorMode::Dark : ColorMode::Light;
    const ui::ScrollbarColors scrollbars = MakeScrollbarColors(definition);
    if (editor_ != nullptr) {
        ApplyColorModeToWindow(editor_->hwnd(), colorMode);
        ui::SetThemedScrollbar(editor_->hwnd(), scrollbars);
        editor_->SetColorScheme(definition.editor);
    }
    ApplyColorModeToWindow(navPanel_.treeHwnd(), colorMode);
    ui::SetThemedScrollbar(navPanel_.treeHwnd(), scrollbars);

    // The preview document embeds the theme CSS, so rebuild it. Hidden tabs
    // defer this until Activate() to avoid re-rendering every open document at
    // once when the theme changes.
    lastHtml_.clear();
    rendered_ = false;
    if (IsWindowVisible()) {
        RenderNow();
    }
    Invalidate(FALSE);
}

void DocumentView::Activate()
{
    if (!IsWindow()) {
        return;
    }

    EnsurePreview();
    if (renderScheduled_) {
        ::KillTimer(m_hWnd, kRenderTimerId);
        renderScheduled_ = false;
        RenderNow();
    } else if (!rendered_) {
        RenderNow();
    }

    ShowWindow(SW_SHOW);
    Layout();
    FocusPrimaryPane();
}

void DocumentView::Deactivate()
{
    if (IsWindow()) {
        if (scrollSync_) {
            scrollSync_->Cancel();
        }
        HideDragLine();
        ShowWindow(SW_HIDE);
    }
}

void DocumentView::SetBounds(const RECT& rc)
{
    if (!IsWindow()) {
        return;
    }
    RECT rect = rc;
    MoveWindow(&rect, TRUE);
    Layout();
}

void DocumentView::FocusPrimaryPane()
{
    if (settings_.showEditor && editor_ != nullptr) {
        editor_->Focus();
    } else if (preview_ != nullptr) {
        preview_->Focus();
    } else if (navPanel_.IsWindow()) {
        navPanel_.FocusTree();
    }
}

void DocumentView::OnDpiChanged()
{
    if (editor_ != nullptr && theme() != nullptr) {
        editor_->SetDpiScale(static_cast<double>(theme()->dpi()) / 96.0);
    }
    if (navPanel_.IsWindow()) {
        navPanel_.OnDpiChanged();
    }
    dragLine_.OnDpiChanged();
    findBar_.OnDpiChanged();
    Layout();
}

bool DocumentView::ForwardAccelerator(MSG* msg)
{
    if (preview_ == nullptr) {
        return false;
    }

    // The IE control's accelerator table must only see keys aimed at it.
    // Forwarding every message lets it swallow Backspace (browser "back"),
    // breaking editing in the find bar or elsewhere.
    const HWND host = preview_->hwnd();
    const HWND focus = ::GetFocus();
    if (host == nullptr || focus == nullptr) {
        return false;
    }
    if (focus != host && ::IsChild(host, focus) == FALSE) {
        return false;
    }
    return preview_->TranslateAccelerator(msg);
}

bool DocumentView::findBarVisible() const
{
    // Tracked separately from IsWindowVisible(): this view may be the hidden
    // tab while a global layout change runs, and the bar must stay open.
    return findBarOpen_ && findBar_.IsWindow();
}

void DocumentView::ShowFindBar()
{
    if (!findBar_.IsWindow()) {
        return;
    }

    const bool wasOpen = findBarOpen_;
    findBarOpen_ = true;
    if (!wasOpen) {
        findBar_.ShowWindow(SW_SHOW);
        Layout();
        if (!findBar_.query().empty()) {
            OnFindQueryChanged(findBar_.query(), findBar_.options());
        }
    }
    findBar_.FocusQuery();
}

void DocumentView::HideFindBar()
{
    if (!findBarOpen_) {
        return;
    }

    findBarOpen_ = false;
    ClearFindHighlights();
    findBar_.SetStatus(L"");
    findBar_.ShowWindow(SW_HIDE);
    Layout();
    FocusPrimaryPane();
}

bool DocumentView::CloseFindBarIfVisible()
{
    if (!findBarOpen_) {
        return false;
    }
    HideFindBar();
    return true;
}

void DocumentView::FindNext()
{
    if (!findBar_.IsWindow()) {
        return;
    }
    if (!findBarVisible()) {
        ShowFindBar();
        return;
    }
    if (findBar_.query().empty()) {
        findBar_.FocusQuery();
        return;
    }
    RunFindStep(1);
}

void DocumentView::FindPrevious()
{
    if (!findBar_.IsWindow()) {
        return;
    }
    if (!findBarVisible()) {
        ShowFindBar();
        return;
    }
    if (findBar_.query().empty()) {
        findBar_.FocusQuery();
        return;
    }
    RunFindStep(-1);
}

void DocumentView::OnFindQueryChanged(const std::wstring& query,
                                      const ui::FindBar::Options& options)
{
    if (query.empty()) {
        ClearFindHighlights();
        findStatus_ = 1;
        findBar_.SetStatus(L"");
        return;
    }

    if (preview_ == nullptr) {
        EnsurePreview();
    }
    if (preview_ == nullptr || !preview_->SupportsJavaScript()) {
        findStatus_ = 2;
        findBar_.SetStatus(L"当前视图不支持查找", ui::FindBar::StatusKind::Error);
        return;
    }

    const std::string result = preview_->RunJavaScriptForResult(
        buildFindScript(query, options), "awm-find-result");
    ApplyFindResult(result);
}

void DocumentView::RunFindStep(int delta)
{
    if (preview_ == nullptr || !preview_->SupportsJavaScript()) {
        return;
    }

    std::string script = kFindStepScript;
    replaceToken(script, "__DELTA__", delta > 0 ? "1" : "-1");
    ApplyFindResult(preview_->RunJavaScriptForResult(script, "awm-find-result"));
}

void DocumentView::ApplyFindResult(const std::string& result)
{
    findMatchCount_ = 0;
    findCurrentIndex_ = -1;

    if (result.empty()) {
        findStatus_ = 3;
        findBar_.SetStatus(L"查找不可用（预览未就绪）", ui::FindBar::StatusKind::Error);
        return;
    }
    if (result == "err") {
        findStatus_ = 4;
        findBar_.SetStatus(L"正则表达式无效", ui::FindBar::StatusKind::Error);
        return;
    }

    int count = 0;
    int index = -1;
    std::istringstream stream(result);
    std::string tag;
    if (!(stream >> tag >> count >> index) || tag != "ok") {
        findStatus_ = 5;
        findBar_.SetStatus(L"查找失败", ui::FindBar::StatusKind::Error);
        return;
    }

    findStatus_ = 6;
    findMatchCount_ = count;
    findCurrentIndex_ = index;
    if (count <= 0) {
        findBar_.SetStatus(L"无结果", ui::FindBar::StatusKind::Error);
        return;
    }
    findBar_.SetStatus(std::to_wstring(index + 1) + L"/" + std::to_wstring(count));
}

void DocumentView::ClearFindHighlights()
{
    findMatchCount_ = 0;
    findCurrentIndex_ = -1;
    findStatus_ = 0;
    if (preview_ != nullptr && preview_->SupportsJavaScript()) {
        preview_->RunJavaScript(kFindClearScript);
    }
}

LRESULT DocumentView::OnTestFindState(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    switch (wParam) {
    case 0:
        return findBarVisible() ? 1 : 0;
    case 1:
        return findMatchCount_;
    case 2:
        return findCurrentIndex_;
    case 3:
        return findStatus_;
    default:
        return 0;
    }
}

LRESULT DocumentView::OnTestPreviewState(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (wParam != 0 || preview_ == nullptr || !preview_->SupportsJavaScript()) {
        return 0;
    }

    static const char* kScript = R"JS(
(function(){
var codes=document.getElementsByTagName('code'),lang=0,spans=0,i,c;
for(i=0;i<codes.length;i++){
c=codes[i];
if(/(?:^|\s)language-/i.test(c.className||'')){lang++;}
spans+=c.getElementsByTagName('span').length;
}
var dark=0;
try{
var bg=window.getComputedStyle?getComputedStyle(document.body).backgroundColor:'';
var m=/rgba?\((\d+)[, ]+(\d+)[, ]+(\d+)/.exec(bg);
if(m){dark=((+m[1])*299+(+m[2])*587+(+m[3])*114)/1000<128?1:0;}
}catch(e){}
var h=document.getElementById('awm-test-preview');
if(!h){h=document.createElement('div');h.id='awm-test-preview';h.style.display='none';document.body.appendChild(h);}
var mode=document.documentMode||0;
var scripts=document.getElementsByTagName('script').length;
h.innerHTML=lang+';'+spans+';'+((window.Prism&&Prism.languages)?1:0)+';'+dark+';'+mode+';'+scripts;
})();
)JS";

    const std::string result =
        preview_->RunJavaScriptForResult(kScript, "awm-test-preview");
    unsigned lang = 0;
    unsigned spans = 0;
    unsigned prism = 0;
    unsigned dark = 0;
    unsigned mode = 0;
    unsigned scripts = 0;
    if (sscanf_s(result.c_str(), "%u;%u;%u;%u;%u;%u", &lang, &spans, &prism, &dark,
                 &mode, &scripts) != 6) {
        return 0;
    }

    const UINT64 packed = (spans & 0xFFFFu) | ((lang & 0xFFu) << 16) |
                          (prism != 0 ? (1u << 24) : 0u) |
                          (dark != 0 ? (1u << 25) : 0u) |
                          ((static_cast<UINT64>(mode) & 0xFFu) << 32) |
                          ((static_cast<UINT64>(scripts) & 0xFFu) << 40);
    return static_cast<LRESULT>(packed);
}

void DocumentView::FlushEditorText()
{
    if (editor_ == nullptr || session_ == nullptr || !textDirty_) {
        return;
    }

    session_->setSource(editor_->GetText());
    textDirty_ = false;
    NotifyStateChanged();
}

bool DocumentView::Save(std::error_code& ec)
{
    if (session_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    FlushEditorText();

    const bool wasDirty = session_->isDirty();
    const bool saved = session_->save(ec);
    if (saved && wasDirty) {
        NotifyStateChanged();
    }
    return saved;
}

bool DocumentView::SaveAs(const std::wstring& path, std::error_code& ec)
{
    if (session_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    FlushEditorText();

    const bool saved = session_->saveAs(path, ec);
    if (saved) {
        reportedDirty_ = session_->isDirty();
        // The title may have changed even though the dirty state did not.
        if (callbacks_.onStateChanged) {
            callbacks_.onStateChanged(*this);
        }
    }
    return saved;
}

void DocumentView::Reload(std::unique_ptr<DocumentSession> session)
{
    if (session == nullptr || session_ == nullptr || editor_ == nullptr ||
        services_.highlighter == nullptr) {
        return;
    }

    if (renderScheduled_) {
        ::KillTimer(m_hWnd, kRenderTimerId);
        renderScheduled_ = false;
    }

    const int firstVisible = editor_->FirstVisibleLine();

    // SetText() emits SCN_MODIFIED, which would otherwise mark the fresh
    // session dirty as if the user had edited it.
    suppressEditorChanges_ = true;
    session_ = std::move(session);
    const std::string& source = session_->source();
    editor_->SetText(source);
    editor_->ApplyHighlighting(source, services_.highlighter->highlight(source));
    suppressEditorChanges_ = false;

    textDirty_ = false;
    rendered_ = false;
    lastHtml_.clear();
    RenderNow();
    editor_->SetFirstVisibleLine(firstVisible);

    // The tab title comes from the first heading; it may have changed on disk.
    reportedDirty_ = session_->isDirty();
    if (callbacks_.onStateChanged) {
        callbacks_.onStateChanged(*this);
    }
}

void DocumentView::EnsurePreview()
{
    if (preview_ != nullptr || !services_.createPreview || !IsWindow()) {
        return;
    }

    preview_ = services_.createPreview();
    if (!preview_) {
        return;
    }

    RECT zero = {0, 0, 0, 0};
    if (!preview_->Create(m_hWnd, zero)) {
        preview_.reset();
        return;
    }

    PreviewCallbacks callbacks;
    callbacks.onLink = services_.onOpenLink;
    callbacks.onScrolledToLine = [this](double line) {
        if (scrollSync_) {
            scrollSync_->OnPreviewScrolled(line);
        }
    };
    preview_->SetCallbacks(std::move(callbacks));
    preview_->SetZoom(100);
    if (scrollSync_) {
        scrollSync_->SetPreview(preview_.get());
    }
    rendered_ = false;
}

void DocumentView::Layout()
{
    if (!IsWindow() || theme() == nullptr) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    const ui::RectI area{0, 0, static_cast<int>(client.right),
                         static_cast<int>(client.bottom)};
    const int divider = theme()->px(6);

    const ui::NavLayout nav =
        ui::layoutWithNav(area, settings_.showNav, theme()->px(settings_.navWidth), divider,
                          theme()->px(160), theme()->px(480), theme()->px(260));
    const bool navVisible = settings_.showNav && nav.dividerVisible && !nav.nav.empty();

    // WM_SIZE is delivered while CWindowImpl::Create is still running, before
    // the editor/nav/splitter children exist. Guard every child operation so
    // Debug ATL assertions (CWindow::ShowWindow on a null HWND) cannot fire.
    if (navPanel_.IsWindow()) {
        navPanel_.ShowWindow(navVisible ? SW_SHOW : SW_HIDE);
    }
    if (navSplitter_.IsWindow()) {
        navSplitter_.ShowWindow(navVisible ? SW_SHOW : SW_HIDE);
    }
    if (navVisible) {
        if (navPanel_.IsWindow()) {
            RECT navRect = toRect(nav.nav);
            navPanel_.MoveWindow(&navRect, TRUE);

            // The container repaint and child layout can race during split
            // changes; explicitly refresh the nav panel and its tree so a
            // stale (unpainted) sidebar cannot remain on screen.
            ::RedrawWindow(navPanel_.m_hWnd, nullptr, nullptr,
                           RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        if (navSplitter_.IsWindow()) {
            RECT splitRect = toRect(nav.divider);
            navSplitter_.MoveWindow(&splitRect, TRUE);
        }
    }

    RECT content = toRect(nav.content);
    ui::RectI contentArea = nav.content;

    // The find bar spans the document area (editor + preview) and pushes both
    // panes down while it is open.
    if (findBar_.IsWindow()) {
        if (findBarVisible()) {
            const int barHeight = findBar_.preferredHeight();
            RECT barRect = content;
            barRect.bottom = barRect.top + barHeight;
            findBar_.MoveWindow(&barRect, TRUE);
            findBar_.ShowWindow(SW_SHOW);
            contentArea.y += barHeight;
            contentArea.h = std::max(0, contentArea.h - barHeight);
            content.top += barHeight;
            content.bottom = std::max(content.top, content.bottom);
        } else {
            findBar_.ShowWindow(SW_HIDE);
        }
    }

    const bool editorVisible =
        settings_.showEditor && editor_ != nullptr && !contentArea.empty();
    if (editorVisible) {
        const int firstSize =
            settings_.split == ui::Orientation::Horizontal
                ? static_cast<int>(std::lround(contentArea.w * settings_.sideBySideRatio))
                : static_cast<int>(std::lround(contentArea.h * settings_.stackedRatio));
        const ui::SplitLayout split =
            ui::splitArea(contentArea, settings_.split, firstSize, divider, theme()->px(120),
                          theme()->px(120));

        // Side by side: the editor sits on the left and the preview in the
        // middle (the nav stays on the right), so sideBySideRatio sizes the
        // editor. Stacked keeps the preview above the editor, so stackedRatio
        // sizes the preview.
        const bool editorFirst = settings_.split == ui::Orientation::Horizontal;
        if (preview_ != nullptr) {
            RECT previewRect = toRect(editorFirst ? split.second : split.first);
            preview_->SetBounds(previewRect);
        }
        RECT editorRect = toRect(editorFirst ? split.first : split.second);
        editor_->SetBounds(editorRect);
        editor_->Show(true);

        if (editorSplitter_.IsWindow()) {
            editorSplitter_.SetAxis(settings_.split == ui::Orientation::Horizontal
                                        ? ui::SplitterAxis::Vertical
                                        : ui::SplitterAxis::Horizontal);
            editorSplitter_.ShowWindow(SW_SHOW);
            RECT splitRect = toRect(split.divider);
            editorSplitter_.MoveWindow(&splitRect, TRUE);
        }
    } else {
        if (preview_ != nullptr) {
            RECT previewRect = content;
            preview_->SetBounds(previewRect);
        }
        if (editor_ != nullptr) {
            editor_->Show(false);
        }
        if (editorSplitter_.IsWindow()) {
            editorSplitter_.ShowWindow(SW_HIDE);
        }
    }

    Invalidate(FALSE);
}

void DocumentView::ScheduleRender()
{
    if (renderScheduled_) {
        return;
    }
    renderScheduled_ = true;
    ::SetTimer(m_hWnd, kRenderTimerId, kRenderDelayMs, nullptr);
}

void DocumentView::RenderNow()
{
    if (session_ == nullptr || services_.engine == nullptr || services_.highlighter == nullptr ||
        editor_ == nullptr) {
        return;
    }

    // Editing invalidates any queued scroll synchronization.
    if (scrollSync_) {
        scrollSync_->Suspend(300);
    }

    if (textDirty_) {
        FlushEditorText();
    }

    const std::string& source = session_->source();
    const RenderResult result = services_.engine->render(source, RenderOptions{});
    const bool outlineChanged = !outlineEquals(currentOutline_, result.outline);

    session_->setOutline(result.outline);
    if (outlineChanged) {
        currentOutline_ = result.outline;
        navPanel_.SetItems(toNavItems(currentOutline_));
    }

    editor_->ApplyHighlighting(source, services_.highlighter->highlight(source));

    if (preview_ != nullptr) {
        PreviewHtmlOptions options;
        options.baseFileUrl = BaseUrl();
        options.theme = &theme::ResolveTheme(settings_.theme);
        if (services_.previewAssets != nullptr) {
            options.inlineScript = services_.previewAssets->prismScript;
        }
        std::string html = BuildPreviewHtml(result.bodyHtml, options);
        if (html != lastHtml_) {
            lastHtml_ = std::move(html);
            preview_->SetHtml(lastHtml_, options.baseFileUrl);
        }
    }

    // The preview document was rewritten (or the tab was activated); restore
    // the find highlights on the fresh DOM.
    if (preview_ != nullptr && findBarVisible() && !findBar_.query().empty()) {
        OnFindQueryChanged(findBar_.query(), findBar_.options());
    }

    textDirty_ = false;
    rendered_ = preview_ != nullptr;
}

void DocumentView::OnEditorChanged()
{
    if (session_ == nullptr || suppressEditorChanges_) {
        return;
    }
    textDirty_ = true;
    session_->markDirty();
    NotifyStateChanged();
    ScheduleRender();
}

void DocumentView::OnNavSelected(std::size_t index)
{
    if (index >= currentOutline_.size()) {
        return;
    }

    if (renderScheduled_) {
        ::KillTimer(m_hWnd, kRenderTimerId);
        renderScheduled_ = false;
        RenderNow();
    }

    // TOC navigation positions both panes explicitly; ignore the scroll
    // events it causes and cancel any queued sync so it cannot fight it.
    if (scrollSync_) {
        scrollSync_->Suspend(250);
    }

    const OutlineItem& item = currentOutline_[index];
    if (preview_ != nullptr) {
        preview_->ScrollToAnchor(item.anchor);
    }
    if (editor_ != nullptr) {
        editor_->GotoLine(item.line);
    }
    navPanel_.SetSelected(index);
}

void DocumentView::PreviewSplitterDrag(bool editorSplitter, int position)
{
    if (theme() == nullptr) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    const ui::RectI area{0, 0, static_cast<int>(client.right),
                         static_cast<int>(client.bottom)};
    const int divider = theme()->px(6);
    const ui::NavLayout nav =
        ui::layoutWithNav(area, settings_.showNav, theme()->px(settings_.navWidth), divider,
                          theme()->px(160), theme()->px(480), theme()->px(260));

    if (editorSplitter) {
        if (nav.content.empty()) {
            dragLine_.Hide();
            return;
        }

        if (settings_.split == ui::Orientation::Horizontal) {
            const int minX = nav.content.x + static_cast<int>(nav.content.w * 0.2);
            const int maxX = nav.content.x + static_cast<int>(nav.content.w * 0.8);
            const int x = std::clamp(position, minX, maxX);
            dragLine_.ShowVertical(x, nav.content.y, nav.content.bottom());
        } else {
            const int minY = nav.content.y + static_cast<int>(nav.content.h * 0.2);
            const int maxY = nav.content.y + static_cast<int>(nav.content.h * 0.8);
            const int y = std::clamp(position, minY, maxY);
            dragLine_.ShowHorizontal(y, nav.content.x, nav.content.right());
        }
        return;
    }

    const int minNav = theme()->px(160);
    const int maxNav =
        std::min(theme()->px(480), std::max(0, static_cast<int>(client.right) - divider -
                                                   theme()->px(260)));
    if (maxNav < minNav) {
        dragLine_.Hide();
        return;
    }
    const int minPosition = static_cast<int>(client.right) - maxNav;
    const int maxPosition = static_cast<int>(client.right) - minNav;
    const int x = std::clamp(position, minPosition, maxPosition);
    dragLine_.ShowVertical(x, 0, static_cast<int>(client.bottom));
}

void DocumentView::CommitSplitterDrag(bool editorSplitter, int position)
{
    dragLine_.Hide();
    if (theme() == nullptr) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    const ui::RectI area{0, 0, static_cast<int>(client.right),
                         static_cast<int>(client.bottom)};
    const int divider = theme()->px(6);
    const ui::NavLayout nav =
        ui::layoutWithNav(area, settings_.showNav, theme()->px(settings_.navWidth), divider,
                          theme()->px(160), theme()->px(480), theme()->px(260));

    if (editorSplitter) {
        if (nav.content.empty()) {
            return;
        }

        double ratio = 0.5;
        if (settings_.split == ui::Orientation::Horizontal) {
            ratio = static_cast<double>(position - nav.content.x) /
                    static_cast<double>(nav.content.w);
        } else {
            ratio = static_cast<double>(position - nav.content.y) /
                    static_cast<double>(nav.content.h);
        }
        ratio = std::clamp(ratio, 0.2, 0.8);
        if (settings_.split == ui::Orientation::Horizontal) {
            settings_.sideBySideRatio = ratio;
        } else {
            settings_.stackedRatio = ratio;
        }
    } else {
        const int navPixels = static_cast<int>(client.right) - position;
        const int logical = static_cast<int>(
            std::lround(static_cast<double>(navPixels) * 96.0 / theme()->dpi()));
        settings_.navWidth = std::clamp(logical, 160, 480);
    }

    // Apply the new layout exactly once, after the mouse is released.
    Layout();
    NotifyViewSettingsChanged();

    // The nav panel hosts a themed, double-buffered tree. Repaint it after the
    // whole commit message burst (Layout -> settings fan-out -> MainWindow
    // layout) has finished; a synchronous repaint inside the mouse-up handler
    // races with those moves and can leave the tree half painted.
    ::PostMessage(m_hWnd, kNavRepaintMessage, 0, 0);
}

LRESULT DocumentView::OnNavRepaint(UINT, WPARAM, LPARAM, BOOL&)
{
    if (!navPanel_.IsWindow()) {
        return 0;
    }
    ::RedrawWindow(navPanel_.m_hWnd, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    return 0;
}

void DocumentView::HideDragLine()
{
    dragLine_.Hide();
}

void DocumentView::NotifyStateChanged()
{
    if (session_ == nullptr) {
        return;
    }
    const bool dirty = session_->isDirty();
    if (dirty != reportedDirty_) {
        reportedDirty_ = dirty;
        if (callbacks_.onStateChanged) {
            callbacks_.onStateChanged(*this);
        }
    }
}

void DocumentView::NotifyViewSettingsChanged()
{
    if (callbacks_.onViewSettingsChanged) {
        callbacks_.onViewSettingsChanged(settings_);
    }
}

std::string DocumentView::BaseUrl() const
{
    if (session_ == nullptr) {
        return {};
    }
    const std::wstring directory = fileDirectory(session_->path());
    return directory.empty() ? std::string() : fileUrlFromPath(directory, true);
}

LRESULT DocumentView::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    Layout();
    return 0;
}

LRESULT DocumentView::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (wParam == kRenderTimerId) {
        ::KillTimer(m_hWnd, kRenderTimerId);
        renderScheduled_ = false;
        RenderNow();
    } else if (wParam == kScrollSyncTimerId) {
        ::KillTimer(m_hWnd, kScrollSyncTimerId);
        if (scrollSync_) {
            scrollSync_->OnTimerTick();
        }
    }
    return 0;
}

LRESULT DocumentView::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);
    const ui::Palette palette = theme() != nullptr ? theme()->palette() : ui::Palette{};
    HBRUSH brush = ::CreateSolidBrush(ui::ToColorRef(palette.windowBg));
    if (brush != nullptr) {
        ::FillRect(dc, &client, brush);
        ::DeleteObject(brush);
    }

    EndPaint(&ps);
    return 0;
}

LRESULT DocumentView::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT DocumentView::OnNotify(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    // Scintilla sends SCN_MODIFIED / SCN_UPDATEUI to its parent window, so the
    // notification must be forwarded to the editor implementation from here.
    auto* header = reinterpret_cast<LPNMHDR>(lParam);
    if (header != nullptr && editor_ != nullptr && header->hwndFrom == editor_->hwnd()) {
        editor_->HandleHostNotification(header);
    }
    return 0;
}

} // namespace awm
