#pragma once

#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlctrls.h>
#include <atlwin.h>

#include <memory>
#include <string>
#include <vector>

#include "appsettings.h"
#include "commands.h"
#include "controls/tabstrip.h"
#include "document/documentformatregistry.h"
#include "documentview.h"
#include "highlight/pmhhighlighter.h"
#include "history/recent_files.h"
#include "markdown/cmarkengine.h"
#include "settings/isettings.h"
#include "theme.h"
#include "viewstate.h"

namespace awm {

/// Application main window: tab strip, active document view, menus and
/// persistence. Model state (documents, recent files) lives in app/core classes.
class MainWindow : public CWindowImpl<MainWindow, CWindow, CFrameWinTraits>,
                   public CMessageFilter
{
public:
    DECLARE_WND_CLASS(L"ActiveXWebMarkMainWindow")

    MainWindow();
    ~MainWindow() override;

    BOOL PreTranslateMessage(MSG* pMsg) override;

    void OpenFile(const std::wstring& path);
    void OpenFiles(const std::vector<std::wstring>& paths);
    bool startMaximized() const { return startMaximized_; }

    BEGIN_MSG_MAP(MainWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
        MESSAGE_HANDLER(WM_INITMENUPOPUP, OnInitMenuPopup)
        COMMAND_ID_HANDLER(cmd::kFileOpen, OnFileOpen)
        COMMAND_ID_HANDLER(cmd::kFileSave, OnFileSave)
        COMMAND_ID_HANDLER(cmd::kFileSaveAs, OnFileSaveAs)
        COMMAND_ID_HANDLER(cmd::kFileCloseTab, OnFileCloseTab)
        COMMAND_ID_HANDLER(cmd::kFileExit, OnFileExit)
        COMMAND_ID_HANDLER(cmd::kViewEditor, OnViewEditor)
        COMMAND_ID_HANDLER(cmd::kViewNav, OnViewNav)
        COMMAND_ID_HANDLER(cmd::kViewSplitSideBySide, OnViewSplitSideBySide)
        COMMAND_ID_HANDLER(cmd::kViewSplitStacked, OnViewSplitStacked)
        COMMAND_ID_HANDLER(cmd::kViewToggleSplit, OnViewToggleSplit)
        COMMAND_ID_HANDLER(cmd::kRecentMore, OnRecentMore)
        COMMAND_ID_HANDLER(cmd::kRecentClear, OnRecentClear)
        COMMAND_ID_HANDLER(cmd::kHelpAbout, OnHelpAbout)
        COMMAND_RANGE_HANDLER(cmd::kRecentBase, cmd::kRecentBase + cmd::kRecentCount - 1,
                              OnRecentFile)
    END_MSG_MAP()

private:
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnInitMenuPopup(UINT, WPARAM, LPARAM, BOOL&);

    LRESULT OnFileOpen(WORD, WORD, HWND, BOOL&);
    LRESULT OnFileSave(WORD, WORD, HWND, BOOL&);
    LRESULT OnFileSaveAs(WORD, WORD, HWND, BOOL&);
    LRESULT OnFileCloseTab(WORD, WORD, HWND, BOOL&);
    LRESULT OnFileExit(WORD, WORD, HWND, BOOL&);
    LRESULT OnViewEditor(WORD, WORD, HWND, BOOL&);
    LRESULT OnViewNav(WORD, WORD, HWND, BOOL&);
    LRESULT OnViewSplitSideBySide(WORD, WORD, HWND, BOOL&);
    LRESULT OnViewSplitStacked(WORD, WORD, HWND, BOOL&);
    LRESULT OnViewToggleSplit(WORD, WORD, HWND, BOOL&);
    LRESULT OnRecentFile(WORD, WORD, HWND, BOOL&);
    LRESULT OnRecentMore(WORD, WORD, HWND, BOOL&);
    LRESULT OnRecentClear(WORD, WORD, HWND, BOOL&);
    LRESULT OnHelpAbout(WORD, WORD, HWND, BOOL&);

    DocumentView* ActiveView() const;
    DocumentViewServices MakeServices();
    void SendCommand(int id);

    int FindTabByPath(const std::wstring& path) const;
    int OpenFileInternal(const std::wstring& path, bool activate);
    void ActivateTab(int index);
    void CloseTab(int index);
    void CycleTab(int delta);

    bool ConfirmClose(DocumentView& view);
    bool ConfirmCloseAll();
    bool SaveView(DocumentView& view);
    bool SaveViewAs(DocumentView& view);
    bool SaveActive();
    bool SaveActiveAs();

    void ApplyViewSettingsToAll();
    void UpdateTabStrip();
    void UpdateWindowTitle();
    void UpdateMenuState();
    void CreateMenuBar();
    void RebuildRecentMenu();
    void Layout();
    void PaintWelcome(HDC dc, const RECT& client);
    void SaveSettings();

    DocumentFormatRegistry registry_;
    CmarkEngine engine_;
    PmhSyntaxHighlighter highlighter_;
    std::unique_ptr<ISettings> settings_;
    RecentFiles recent_;

    ui::Theme theme_;
    ui::TabStrip tabStrip_;
    std::vector<std::unique_ptr<DocumentView>> views_;
    int activeIndex_ = -1;
    ViewSettings viewSettings_;

    HMENU mainMenu_ = nullptr;
    HMENU fileMenu_ = nullptr;
    HMENU viewMenu_ = nullptr;
    HMENU recentMenu_ = nullptr;

    WindowPlacementState placement_;
    bool startMaximized_ = false;
};

} // namespace awm
