#pragma once

#include "wtlinclude.h"
#include "theme.h"

namespace awm::ui {

/// Thin accent line shown while a splitter is being dragged.
///
/// This is a top-level, non-activating popup owned by the host's root window,
/// not a child of the host. Moving a child window across the panes (the native
/// tree view, Scintilla, MSHTML) invalidates the parent window rather than the
/// grandchild control that actually owns the pixels, and the panes clip their
/// own painting; the exposed strip is then left unpainted and shows up as a
/// ghost line. A popup is composited by DWM, so moving it never asks the
/// application's windows to repaint at all.
///
/// The real panes are not resized during the drag either; this control
/// provides the live feedback and is hidden when the drag ends. Keeping the
/// panes static avoids continuous IE/Scintilla relayout and flicker.
class DragLine : public CWindowImpl<DragLine> {
public:
    DECLARE_WND_CLASS_EX(L"AwmDragLine", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    ~DragLine();

    /// Creates the hidden overlay owned by \a host's top-level window.
    bool Create(HWND host);
    void SetTheme(const Theme* theme);
    /// Coordinates are relative to \a host's client area.
    void ShowVertical(int x, int top, int bottom);
    /// Coordinates are relative to \a host's client area.
    void ShowHorizontal(int y, int left, int right);
    void Hide();
    void OnDpiChanged();

    BEGIN_MSG_MAP(DragLine)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
        MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
    END_MSG_MAP()

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNcHitTest(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseActivate(UINT, WPARAM, LPARAM, BOOL&);
    int thickness() const;
    void showClientRect(int x, int y, int width, int height);

    const Theme* theme_ = nullptr;
    HWND host_ = nullptr;
};

} // namespace awm::ui
