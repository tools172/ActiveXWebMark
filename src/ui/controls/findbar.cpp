#include "controls/findbar.h"

#include <algorithm>
#include <utility>

#include <commctrl.h>

namespace awm::ui {

namespace {

constexpr COLORREF kFallbackBackground = RGB(255, 255, 255);

RECT makeRect(int left, int top, int right, int bottom)
{
    return RECT{left, top, right, bottom};
}

} // namespace

FindBar::~FindBar()
{
    if (backgroundBrush_ != nullptr) {
        ::DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
}

bool FindBar::Create(HWND parent, const RECT& rc, UINT id)
{
    RECT rect = rc;
    if (CWindowImpl<FindBar>::Create(parent, rect, nullptr, WS_CHILD | WS_CLIPCHILDREN, 0, id) ==
        nullptr) {
        return false;
    }

    RECT zero = {0, 0, 0, 0};
    query_.Create(m_hWnd, zero, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                  kQueryId);
    ::SetWindowSubclass(query_.m_hWnd, &FindBar::QueryEditProc, kEditSubclassId,
                        reinterpret_cast<DWORD_PTR>(this));
    // The edit is borderless inside our own rounded frame; internal margins keep
    // the caret away from the frame edge.
    ::SendMessageW(query_.m_hWnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                   MAKELPARAM(8, 8));

    const DWORD toggleStyle =
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_PUSHLIKE;
    matchCase_.Create(m_hWnd, zero, L"Aa", toggleStyle, 0, kMatchCaseId);
    wholeWord_.Create(m_hWnd, zero, L"全词", toggleStyle, 0, kWholeWordId);
    regex_.Create(m_hWnd, zero, L".*", toggleStyle, 0, kRegexId);

    const DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
    previous_.Create(m_hWnd, zero, L"\x2191", buttonStyle, 0, kPreviousId);
    next_.Create(m_hWnd, zero, L"\x2193", buttonStyle, 0, kNextId);
    close_.Create(m_hWnd, zero, L"\x00D7", buttonStyle, 0, kCloseId);
    return true;
}

void FindBar::SetTheme(const Theme* theme)
{
    theme_ = theme;

    const COLORREF background =
        theme_ != nullptr ? ToColorRef(theme_->palette().surface) : kFallbackBackground;
    if (backgroundBrush_ != nullptr) {
        ::DeleteObject(backgroundBrush_);
    }
    backgroundBrush_ = ::CreateSolidBrush(background);

    if (theme_ != nullptr) {
        const HFONT font = theme_->uiFont();
        const HFONT smallFont = theme_->smallFont();
        if (query_.IsWindow()) {
            query_.SetFont(font);
        }
        if (matchCase_.IsWindow()) {
            matchCase_.SetFont(smallFont);
        }
        if (wholeWord_.IsWindow()) {
            wholeWord_.SetFont(smallFont);
        }
        if (regex_.IsWindow()) {
            regex_.SetFont(smallFont);
        }
        if (previous_.IsWindow()) {
            previous_.SetFont(font);
        }
        if (next_.IsWindow()) {
            next_.SetFont(font);
        }
        if (close_.IsWindow()) {
            close_.SetFont(font);
        }
    }

    layoutChildren();
    if (IsWindow()) {
        Invalidate(FALSE);
    }
}

void FindBar::SetCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

int FindBar::preferredHeight() const
{
    return theme_ != nullptr ? theme_->px(38) : 38;
}

void FindBar::FocusQuery()
{
    if (!query_.IsWindow()) {
        return;
    }
    query_.SetFocus();
    query_.SetSel(0, -1);
}

std::wstring FindBar::query() const
{
    if (!query_.IsWindow()) {
        return {};
    }
    const int length = query_.GetWindowTextLength();
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    query_.GetWindowText(&text[0], length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

FindBar::Options FindBar::options() const
{
    Options options;
    options.matchCase = matchCase_.IsWindow() && matchCase_.GetCheck() == BST_CHECKED;
    options.wholeWord = wholeWord_.IsWindow() && wholeWord_.GetCheck() == BST_CHECKED;
    options.regex = regex_.IsWindow() && regex_.GetCheck() == BST_CHECKED;
    return options;
}

void FindBar::SetStatus(const std::wstring& text, StatusKind kind)
{
    if (statusText_ == text && statusKind_ == kind) {
        return;
    }
    statusText_ = text;
    statusKind_ = kind;
    if (IsWindow()) {
        InvalidateRect(&statusRect_, FALSE);
    }
}

void FindBar::OnDpiChanged()
{
    if (IsWindow()) {
        SetTheme(theme_);
    }
}

void FindBar::layoutChildren()
{
    if (!IsWindow() || theme_ == nullptr) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    const int width = static_cast<int>(client.right);
    const int height = static_cast<int>(client.bottom);
    if (width <= 0 || height <= 0) {
        return;
    }

    const int padding = theme_->px(10);
    const int gap = theme_->px(6);
    const int controlHeight = theme_->px(24);
    const int top = std::max(0, (height - controlHeight) / 2);
    const int toggleWidth = theme_->px(34);
    const int wholeWordWidth = theme_->px(46);
    const int arrowWidth = theme_->px(30);
    const int closeWidth = theme_->px(30);

    int right = width - padding;
    const RECT closeRect = makeRect(right - closeWidth, top, right, top + controlHeight);
    right -= closeWidth + gap;
    const RECT nextRect = makeRect(right - arrowWidth, top, right, top + controlHeight);
    right -= arrowWidth + gap;
    const RECT previousRect = makeRect(right - arrowWidth, top, right, top + controlHeight);
    right -= arrowWidth + gap;

    const int togglesWidth = toggleWidth * 2 + wholeWordWidth + gap * 3;
    const int statusMinWidth = theme_->px(70);
    const int editMax = right - gap - statusMinWidth - togglesWidth - gap - padding;
    int editWidth = std::min(theme_->px(300), std::max(theme_->px(120), (width - padding * 2) / 3));
    editWidth = std::min(editWidth, std::max(theme_->px(80), editMax));

    int x = padding;
    queryFrame_ = makeRect(x, top, x + editWidth, top + controlHeight);
    RECT editRect = queryFrame_;
    ::InflateRect(&editRect, -1, -1);
    if (query_.IsWindow()) {
        query_.MoveWindow(&editRect, TRUE);
    }
    x += editWidth + gap;

    if (matchCase_.IsWindow()) {
        RECT rect = makeRect(x, top, x + toggleWidth, top + controlHeight);
        matchCase_.MoveWindow(&rect, TRUE);
    }
    x += toggleWidth + gap;
    if (wholeWord_.IsWindow()) {
        RECT rect = makeRect(x, top, x + wholeWordWidth, top + controlHeight);
        wholeWord_.MoveWindow(&rect, TRUE);
    }
    x += wholeWordWidth + gap;
    if (regex_.IsWindow()) {
        RECT rect = makeRect(x, top, x + toggleWidth, top + controlHeight);
        regex_.MoveWindow(&rect, TRUE);
    }
    x += toggleWidth + gap;

    statusRect_ = makeRect(x, top, std::max(x, right - gap), top + controlHeight);
    if (previous_.IsWindow()) {
        RECT rect = previousRect;
        previous_.MoveWindow(&rect, TRUE);
    }
    if (next_.IsWindow()) {
        RECT rect = nextRect;
        next_.MoveWindow(&rect, TRUE);
    }
    if (close_.IsWindow()) {
        RECT rect = closeRect;
        close_.MoveWindow(&rect, TRUE);
    }
}

void FindBar::notifySearch()
{
    if (callbacks_.onSearch) {
        callbacks_.onSearch(query(), options());
    }
}

LRESULT FindBar::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);
    if (dc == nullptr) {
        return 0;
    }

    RECT client = {};
    GetClientRect(&client);
    HBRUSH background = backgroundBrush_ != nullptr
                            ? backgroundBrush_
                            : static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    ::FillRect(dc, &client, background);

    if (theme_ != nullptr) {
        const Palette& palette = theme_->palette();

        RECT divider = client;
        divider.top = std::max(divider.top, divider.bottom - 1);
        HBRUSH dividerBrush = ::CreateSolidBrush(ToColorRef(palette.divider));
        if (dividerBrush != nullptr) {
            ::FillRect(dc, &divider, dividerBrush);
            ::DeleteObject(dividerBrush);
        }

        if (queryFrame_.right > queryFrame_.left && queryFrame_.bottom > queryFrame_.top) {
            HPEN pen = ::CreatePen(PS_SOLID, 1, ToColorRef(palette.divider));
            const HGDIOBJ oldPen = ::SelectObject(dc, pen);
            const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
            const int radius = theme_->px(6);
            ::RoundRect(dc, queryFrame_.left, queryFrame_.top, queryFrame_.right,
                        queryFrame_.bottom, radius, radius);
            ::SelectObject(dc, oldBrush);
            ::SelectObject(dc, oldPen);
            ::DeleteObject(pen);
        }

        if (!statusText_.empty() && statusRect_.right > statusRect_.left) {
            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, ToColorRef(statusKind_ == StatusKind::Error ? palette.danger
                                                                          : palette.textSecondary));
            const HGDIOBJ oldFont = ::SelectObject(dc, theme_->smallFont());
            RECT textRect = statusRect_;
            ::DrawTextW(dc, statusText_.c_str(), -1, &textRect,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            ::SelectObject(dc, oldFont);
        }
    }

    EndPaint(&ps);
    return 0;
}

LRESULT FindBar::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT FindBar::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    layoutChildren();
    Invalidate(FALSE);
    return 0;
}

LRESULT FindBar::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (wParam == kSearchTimerId) {
        ::KillTimer(m_hWnd, kSearchTimerId);
        // The bar may have been closed before the debounce elapsed; searching
        // then would leave highlights in a hidden view.
        if (IsWindowVisible()) {
            notifySearch();
        }
    }
    return 0;
}

LRESULT FindBar::OnCtlColorEdit(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    HDC dc = reinterpret_cast<HDC>(wParam);
    if (theme_ != nullptr) {
        ::SetTextColor(dc, ToColorRef(theme_->palette().textPrimary));
        ::SetBkColor(dc, ToColorRef(theme_->palette().surface));
    }
    return reinterpret_cast<LRESULT>(backgroundBrush_);
}

LRESULT FindBar::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    ::KillTimer(m_hWnd, kSearchTimerId);
    return 0;
}

LRESULT FindBar::OnQueryChanged(WORD, WORD, HWND, BOOL&)
{
    // Debounce typing so a search runs once the user pauses, not on every key.
    ::SetTimer(m_hWnd, kSearchTimerId, kSearchDelayMs, nullptr);
    return 0;
}

LRESULT FindBar::OnOptionChanged(WORD, WORD, HWND, BOOL&)
{
    ::KillTimer(m_hWnd, kSearchTimerId);
    notifySearch();
    return 0;
}

LRESULT FindBar::OnPrevious(WORD, WORD, HWND, BOOL&)
{
    if (callbacks_.onPrevious) {
        callbacks_.onPrevious();
    }
    return 0;
}

LRESULT FindBar::OnNext(WORD, WORD, HWND, BOOL&)
{
    if (callbacks_.onNext) {
        callbacks_.onNext();
    }
    return 0;
}

LRESULT FindBar::OnClose(WORD, WORD, HWND, BOOL&)
{
    if (callbacks_.onClose) {
        callbacks_.onClose();
    }
    return 0;
}

LRESULT CALLBACK FindBar::QueryEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR subclassId, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<FindBar*>(refData);
    if (msg == WM_KEYDOWN && self != nullptr) {
        if (wParam == VK_RETURN) {
            const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (shift) {
                if (self->callbacks_.onPrevious) {
                    self->callbacks_.onPrevious();
                }
            } else if (self->callbacks_.onNext) {
                self->callbacks_.onNext();
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (self->callbacks_.onClose) {
                self->callbacks_.onClose();
            }
            return 0;
        }
    }
    if (msg == WM_NCDESTROY) {
        ::RemoveWindowSubclass(hwnd, &FindBar::QueryEditProc, subclassId);
    }
    return ::DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace awm::ui
