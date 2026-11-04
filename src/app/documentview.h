#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include "wtlinclude.h"

#include "controls/dragline.h"
#include "controls/findbar.h"
#include "controls/navpanel.h"
#include "controls/splitter.h"
#include "document/outline.h"
#include "ieditorview.h"
#include "ipreviewview.h"
#include "markdown/imarkdownengine.h"
#include "previewassets.h"
#include "scrollsync.h"
#include "session/document_session.h"
#include "theme.h"
#include "viewstate.h"

namespace awm {

class DocumentView;

/// Dependencies injected by MainWindow.
struct DocumentViewServices {
    IMarkdownEngine* engine = nullptr;
    ISyntaxHighlighter* highlighter = nullptr;
    ui::Theme* theme = nullptr;
    std::function<std::unique_ptr<IEditorView>()> createEditor;
    std::function<std::unique_ptr<IPreviewView>()> createPreview;
    std::function<void(const std::string& url)> onOpenLink;
    /// Prism bundle embedded in the executable; null disables highlighting.
    const PreviewAssets* previewAssets = nullptr;
};

/// Notifications from a document view back to its owner.
struct DocumentViewCallbacks {
    /// Title or dirty state changed (tab strip / window title need refreshing).
    std::function<void(DocumentView&)> onStateChanged;
    /// The user dragged a splitter or collapsed the nav panel.
    std::function<void(const ViewSettings&)> onViewSettingsChanged;
};

/// One tab's content: preview + optional editor + right navigation panel.
///
/// Each DocumentView owns an independent DocumentSession, Scintilla editor and
/// navigation tree. The IE preview is created lazily the first time the tab is
/// activated so opening many files does not create many MSHTML instances.
class DocumentView : public CWindowImpl<DocumentView> {
public:
    DECLARE_WND_CLASS_EX(L"AwmDocumentView", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    static constexpr UINT kRenderTimerId = 1;
    static constexpr UINT kRenderDelayMs = 200;
    static constexpr UINT kScrollSyncTimerId = 2;
    static constexpr UINT kScrollSyncDelayMs = 30;
    // Posted after a splitter drag commits so the nav panel is repainted once
    // the resize/layout message burst has finished.
    static constexpr UINT kNavRepaintMessage = WM_APP + 0x51;
    /// Diagnostic hook used by tests/manual/find-smoke.ps1.
    /// wParam 0: 1 when the find bar is visible. 1: match count. 2: current
    /// index (-1 when none). 3: result code (1 empty query, 2 unsupported,
    /// 3 empty preview result, 4 invalid regex, 5 parse error, 6 ok).
    static constexpr UINT kTestFindStateMessage = WM_APP + 0x52;
    /// Diagnostic hook used by tests/manual/theme-smoke.ps1. wParam 0 returns
    /// a packed preview state:
    ///   bits 0-15  token span count
    ///   bits 16-23 language code block count
    ///   bit 24     Prism loaded
    ///   bit 25     computed body background is dark
    ///   bits 32-39 document.documentMode
    ///   bits 40-47 number of <script> elements in the preview document
    static constexpr UINT kTestPreviewStateMessage = WM_APP + 0x54;

    ~DocumentView() override;

    bool Create(HWND parent, const RECT& rc, std::unique_ptr<DocumentSession> session,
                const DocumentViewServices& services);

    void SetCallbacks(DocumentViewCallbacks callbacks);
    void SetViewSettings(const ViewSettings& settings);
    const ViewSettings& viewSettings() const { return settings_; }

    /// Re-applies the active application theme to the editor, nav tree, find
    /// bar and preview. Call after SetViewSettings has delivered the new theme
    /// id; hidden tabs defer the preview rebuild until they are activated.
    void ApplyTheme();

    /// Shows the tab and creates/renders its preview if needed.
    void Activate();
    void Deactivate();
    void SetBounds(const RECT& rc);
    void FocusPrimaryPane();
    void OnDpiChanged();
    bool ForwardAccelerator(MSG* msg);

    /// Opens the find bar for this document and focuses its query box.
    void ShowFindBar();
    void HideFindBar();
    /// Closes the find bar when it is open; returns true if it closed.
    bool CloseFindBarIfVisible();
    void FindNext();
    void FindPrevious();
    bool findBarVisible() const;

    DocumentSession& session() { return *session_; }
    const DocumentSession& session() const { return *session_; }

    /// Pulls the editor buffer into the session (call before saving).
    void FlushEditorText();

    bool Save(std::error_code& ec);
    bool SaveAs(const std::wstring& path, std::error_code& ec);

    BEGIN_MSG_MAP(DocumentView)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
        MESSAGE_HANDLER(kNavRepaintMessage, OnNavRepaint)
        MESSAGE_HANDLER(kTestFindStateMessage, OnTestFindState)
        MESSAGE_HANDLER(kTestPreviewStateMessage, OnTestPreviewState)
    END_MSG_MAP()

private:
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNotify(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNavRepaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTestFindState(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTestPreviewState(UINT, WPARAM, LPARAM, BOOL&);

    void EnsurePreview();
    void Layout();
    void ScheduleRender();
    void RenderNow();
    void OnEditorChanged();
    void OnNavSelected(std::size_t index);
    void PreviewSplitterDrag(bool editorSplitter, int position);
    void CommitSplitterDrag(bool editorSplitter, int position);
    void HideDragLine();
    void NotifyStateChanged();
    void NotifyViewSettingsChanged();
    void OnFindQueryChanged(const std::wstring& query, const ui::FindBar::Options& options);
    void RunFindStep(int delta);
    void ApplyFindResult(const std::string& result);
    void ClearFindHighlights();
    std::string BaseUrl() const;

    const ui::Theme* theme() const { return services_.theme; }

    std::unique_ptr<DocumentSession> session_;
    DocumentViewServices services_;
    DocumentViewCallbacks callbacks_;

    std::unique_ptr<IEditorView> editor_;
    std::unique_ptr<IPreviewView> preview_;
    std::unique_ptr<ScrollSyncController> scrollSync_;
    ui::NavPanel navPanel_;
    ui::Splitter navSplitter_;
    ui::Splitter editorSplitter_;
    ui::DragLine dragLine_;
    ui::FindBar findBar_;

    ViewSettings settings_;
    Outline currentOutline_;
    std::string lastHtml_;
    int findMatchCount_ = 0;
    int findCurrentIndex_ = -1;
    int findStatus_ = 0;
    bool findBarOpen_ = false;
    bool textDirty_ = false;
    bool rendered_ = false;
    bool renderScheduled_ = false;
    bool reportedDirty_ = false;
};

} // namespace awm
