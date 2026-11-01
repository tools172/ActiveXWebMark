#pragma once

#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlwin.h>
#include <atlctrls.h>

#include <memory>
#include <string>

#include "document/documentformatregistry.h"
#include "document/idocument.h"
#include "ieditorview.h"
#include "highlight/isyntaxhighlighter.h"
#include "markdown/imarkdownengine.h"
#include "ipreviewview.h"

namespace awm {

// Command/control identifiers (unique names to avoid Windows' ID_* macros).
constexpr int kCmdFileOpen = 1001;
constexpr int kCmdViewEditor = 1002;
constexpr int kCmdAppExit = 1003;
constexpr int kNavTreeId = 2001;

/// Main application window: navigation tree + HTML preview, with an optional
/// (default hidden) source editor. Preview-first by design.
class MainWindow : public CWindowImpl<MainWindow, CWindow, CFrameWinTraits>,
                   public CMessageFilter
{
public:
    DECLARE_WND_CLASS(L"ActiveXWebMarkMainWindow")

    MainWindow();

    BOOL PreTranslateMessage(MSG* pMsg) override;

    BEGIN_MSG_MAP(MainWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        COMMAND_ID_HANDLER(kCmdFileOpen, OnFileOpen)
        COMMAND_ID_HANDLER(kCmdViewEditor, OnViewEditor)
        COMMAND_ID_HANDLER(kCmdAppExit, OnAppExit)
        NOTIFY_HANDLER(kNavTreeId, TVN_SELCHANGED, OnNavSelChanged)
    END_MSG_MAP()

    void OpenFile(const std::wstring& path);

private:
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnFileOpen(WORD, WORD, HWND, BOOL&);
    LRESULT OnViewEditor(WORD, WORD, HWND, BOOL&);
    LRESULT OnAppExit(WORD, WORD, HWND, BOOL&);
    LRESULT OnNavSelChanged(int, LPNMHDR, BOOL&);

    void CreateMenuBar();
    void Layout();
    void BuildNavTree(const Outline& outline);
    void RenderDocument();
    void NavigateTo(size_t outlineIndex);

    CTreeViewCtrl navTree_;
    std::unique_ptr<IPreviewView> preview_;
    std::unique_ptr<IEditorView> editor_;
    DocumentFormatRegistry registry_;
    std::unique_ptr<IMarkdownEngine> engine_;
    std::unique_ptr<ISyntaxHighlighter> highlighter_;
    std::unique_ptr<IDocument> document_;
    Outline currentOutline_;
    bool editorVisible_ = false;
};

} // namespace awm
