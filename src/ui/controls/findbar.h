#pragma once

#include <functional>
#include <string>

#include "wtlinclude.h"
#include "theme.h"

namespace awm::ui {

/// Compact find bar: query box, case / whole-word / regex toggles, previous /
/// next navigation and a status label.
///
/// The bar owns only the input state; the owner runs the actual search in the
/// active document view and reports the result through SetStatus().
class FindBar : public CWindowImpl<FindBar> {
public:
    DECLARE_WND_CLASS_EX(L"AwmFindBar", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    struct Options {
        bool matchCase = false;
        bool wholeWord = false;
        bool regex = false;
    };

    enum class StatusKind {
        Info,
        Error,
    };

    struct Callbacks {
        /// Fired when the query or an option changes (debounced while typing).
        std::function<void(const std::wstring&, const Options&)> onSearch;
        std::function<void()> onNext;
        std::function<void()> onPrevious;
        std::function<void()> onClose;
    };

    ~FindBar();

    bool Create(HWND parent, const RECT& rc, UINT id);
    void SetTheme(const Theme* theme);
    void SetCallbacks(Callbacks callbacks);
    int preferredHeight() const;

    /// Shows and selects the current query, ready for typing.
    void FocusQuery();
    std::wstring query() const;
    Options options() const;
    void SetStatus(const std::wstring& text, StatusKind kind = StatusKind::Info);
    void OnDpiChanged();

    static constexpr UINT kQueryId = 1;
    static constexpr UINT kMatchCaseId = 2;
    static constexpr UINT kWholeWordId = 3;
    static constexpr UINT kRegexId = 4;
    static constexpr UINT kPreviousId = 5;
    static constexpr UINT kNextId = 6;
    static constexpr UINT kCloseId = 7;

    BEGIN_MSG_MAP(FindBar)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnCtlColorEdit)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_HANDLER(kQueryId, EN_CHANGE, OnQueryChanged)
        COMMAND_ID_HANDLER(kMatchCaseId, OnOptionChanged)
        COMMAND_ID_HANDLER(kWholeWordId, OnOptionChanged)
        COMMAND_ID_HANDLER(kRegexId, OnOptionChanged)
        COMMAND_ID_HANDLER(kPreviousId, OnPrevious)
        COMMAND_ID_HANDLER(kNextId, OnNext)
        COMMAND_ID_HANDLER(kCloseId, OnClose)
    END_MSG_MAP()

private:
    static constexpr UINT_PTR kEditSubclassId = 1;
    static constexpr UINT_PTR kSearchTimerId = 1;
    static constexpr UINT kSearchDelayMs = 120;

    static LRESULT CALLBACK QueryEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                          UINT_PTR subclassId, DWORD_PTR refData);

    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCtlColorEdit(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnQueryChanged(WORD, WORD, HWND, BOOL&);
    LRESULT OnOptionChanged(WORD, WORD, HWND, BOOL&);
    LRESULT OnPrevious(WORD, WORD, HWND, BOOL&);
    LRESULT OnNext(WORD, WORD, HWND, BOOL&);
    LRESULT OnClose(WORD, WORD, HWND, BOOL&);

    void layoutChildren();
    void notifySearch();

    const Theme* theme_ = nullptr;
    Callbacks callbacks_;
    CEdit query_;
    CButton matchCase_;
    CButton wholeWord_;
    CButton regex_;
    CButton previous_;
    CButton next_;
    CButton close_;
    HBRUSH backgroundBrush_ = nullptr;
    RECT queryFrame_ = {};
    RECT statusRect_ = {};
    std::wstring statusText_;
    StatusKind statusKind_ = StatusKind::Info;
};

} // namespace awm::ui
