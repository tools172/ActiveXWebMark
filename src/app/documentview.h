#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include "wtlinclude.h"

#include "controls/dragline.h"
#include "controls/navpanel.h"
#include "controls/splitter.h"
#include "document/outline.h"
#include "ieditorview.h"
#include "ipreviewview.h"
#include "markdown/imarkdownengine.h"
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

    ~DocumentView() override;

    bool Create(HWND parent, const RECT& rc, std::unique_ptr<DocumentSession> session,
                const DocumentViewServices& services);

    void SetCallbacks(DocumentViewCallbacks callbacks);
    void SetViewSettings(const ViewSettings& settings);
    const ViewSettings& viewSettings() const { return settings_; }

    /// Shows the tab and creates/renders its preview if needed.
    void Activate();
    void Deactivate();
    void SetBounds(const RECT& rc);
    void FocusPrimaryPane();
    void OnDpiChanged();
    bool ForwardAccelerator(MSG* msg);

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
    END_MSG_MAP()

private:
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNotify(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNavRepaint(UINT, WPARAM, LPARAM, BOOL&);

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

    ViewSettings settings_;
    Outline currentOutline_;
    std::string lastHtml_;
    bool textDirty_ = false;
    bool rendered_ = false;
    bool renderScheduled_ = false;
    bool reportedDirty_ = false;
};

} // namespace awm
