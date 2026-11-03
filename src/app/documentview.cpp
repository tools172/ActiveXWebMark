#include "documentview.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
    Layout();
}

bool DocumentView::ForwardAccelerator(MSG* msg)
{
    return preview_ != nullptr && preview_->TranslateAccelerator(msg);
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
    const bool editorVisible =
        settings_.showEditor && editor_ != nullptr && !nav.content.empty();
    if (editorVisible) {
        const int firstSize =
            settings_.split == ui::Orientation::Horizontal
                ? static_cast<int>(std::lround(nav.content.w * settings_.sideBySideRatio))
                : static_cast<int>(std::lround(nav.content.h * settings_.stackedRatio));
        const ui::SplitLayout split =
            ui::splitArea(nav.content, settings_.split, firstSize, divider, theme()->px(120),
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
        std::string html = BuildPreviewHtml(result.bodyHtml, options);
        if (html != lastHtml_) {
            lastHtml_ = std::move(html);
            preview_->SetHtml(lastHtml_, options.baseFileUrl);
        }
    }

    textDirty_ = false;
    rendered_ = preview_ != nullptr;
}

void DocumentView::OnEditorChanged()
{
    if (session_ == nullptr) {
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
