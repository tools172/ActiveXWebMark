#include "scintillaeditor.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <scintilla/Scintilla.h>
#include <scintilla/SciLexer.h>

#include "controls/filedroptarget.h"
#include "highlight/stylemerge.h"

namespace awm {

namespace {

// Scintilla style numbers used with SCLEX_CONTAINER (host-driven styling).
enum : int {
    StyleNormal = 0,
    StyleHeading = 1,
    StyleEmphasis = 2,
    StyleStrong = 3,
    StyleCode = 4,
    StyleCodeBlock = 5,
    StyleLink = 6,
    StyleImage = 7,
    StyleListMarker = 8,
    StyleBlockQuote = 9,
    StyleHtml = 10,
    StyleHrule = 11,
    StyleComment = 12,
    StyleReference = 13,
};

int StyleIdFor(MarkupStyle style)
{
    switch (style) {
    case MarkupStyle::Heading:    return StyleHeading;
    case MarkupStyle::Emphasis:   return StyleEmphasis;
    case MarkupStyle::Strong:     return StyleStrong;
    case MarkupStyle::Code:       return StyleCode;
    case MarkupStyle::CodeBlock:  return StyleCodeBlock;
    case MarkupStyle::Link:       return StyleLink;
    case MarkupStyle::Image:      return StyleImage;
    case MarkupStyle::ListMarker: return StyleListMarker;
    case MarkupStyle::BlockQuote: return StyleBlockQuote;
    case MarkupStyle::Html:       return StyleHtml;
    case MarkupStyle::Hrule:      return StyleHrule;
    case MarkupStyle::Comment:    return StyleComment;
    case MarkupStyle::Reference:  return StyleReference;
    default:                      return StyleNormal;
    }
}

COLORREF ToColorRef(const theme::ThemeColor& color)
{
    return RGB(color.r, color.g, color.b);
}

} // namespace

ScintillaEditor::ScintillaEditor() : scheme_(theme::DefaultTheme().editor) {}

ScintillaEditor::~ScintillaEditor()
{
    ReleaseFileDropTarget();
    if (hwnd_ != nullptr && ::IsWindow(hwnd_) != FALSE) {
        ::DestroyWindow(hwnd_);
    }
}

bool ScintillaEditor::Create(HWND parent, const RECT& rc)
{
    static bool classesRegistered = false;
    if (!classesRegistered) {
        ::Scintilla_RegisterClasses(::GetModuleHandle(nullptr));
        classesRegistered = true;
    }

    hwnd_ = ::CreateWindowExW(0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                              rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                              parent, nullptr, ::GetModuleHandle(nullptr), nullptr);
    if (hwnd_ == nullptr) {
        return false;
    }

    // Track the instance so the subclass procedure can route messages back.
    ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    originalProc_ = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
        hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ScintillaEditor::SubclassProc)));

    ::SendMessageW(hwnd_, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    SetupStyles();
    return true;
}

void ScintillaEditor::HandleHostNotification(LPNMHDR header)
{
    if (header == nullptr) {
        return;
    }

    if (header->code == SCN_MODIFIED) {
        const auto* notification = reinterpret_cast<SCNotification*>(header);
        if ((notification->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != 0 &&
            changeCallback_) {
            changeCallback_();
        }
    } else if (header->code == SCN_UPDATEUI) {
        const auto* notification = reinterpret_cast<SCNotification*>(header);
        if ((notification->updated & SC_UPDATE_V_SCROLL) != 0) {
            NotifyScrollIfChanged();
        }
        if ((notification->updated &
             (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL | SC_UPDATE_CONTENT)) != 0) {
            // Scintilla repaints its native scrollbars directly; ask the frame
            // painter to put the theme colors back after range/position
            // changes.
            ::RedrawWindow(hwnd_, nullptr, nullptr,
                           RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
        }
        UpdateLineNumberWidth();
    } else if (header->code == SCN_PAINTED) {
        // Scintilla 4.4.6 may not deliver SCN_UPDATEUI for some scroll paths
        // (WM_VSCROLL / SCI_LINESCROLL); painting after a scroll is reliable.
        NotifyScrollIfChanged();
    }
}

void ScintillaEditor::SetText(const std::string& utf8)
{
    ::SendMessageA(hwnd_, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
    ::SendMessageW(hwnd_, SCI_EMPTYUNDOBUFFER, 0, 0);
    UpdateLineNumberWidth();
}

void ScintillaEditor::SetBounds(const RECT& rc)
{
    // Preserve the scroll position when a pane/splitter resize changes the
    // viewport; otherwise the editor jumps back to the top.
    const int firstVisible =
        static_cast<int>(::SendMessageW(hwnd_, SCI_GETFIRSTVISIBLELINE, 0, 0));
    const int xOffset = static_cast<int>(::SendMessageW(hwnd_, SCI_GETXOFFSET, 0, 0));

    ::MoveWindow(hwnd_, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

    ::SendMessageW(hwnd_, SCI_SETFIRSTVISIBLELINE, firstVisible, 0);
    ::SendMessageW(hwnd_, SCI_SETXOFFSET, xOffset, 0);
}

void ScintillaEditor::Show(bool visible)
{
    ::ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
}

std::string ScintillaEditor::GetText() const
{
    const int length = static_cast<int>(::SendMessageA(hwnd_, SCI_GETLENGTH, 0, 0));
    // SCI_GETTEXT writes the trailing NUL as well, so allocate one extra byte
    // and then trim it; writing length+1 bytes into a length-sized string was
    // an out-of-bounds write.
    std::string text(static_cast<std::size_t>(length) + 1, '\0');
    ::SendMessageA(hwnd_, SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void ScintillaEditor::ApplyHighlighting(const std::string& source,
                                        const std::vector<StyleSpan>& spans)
{
    ApplyStyle(source, spans);
}

void ScintillaEditor::GotoLine(int line)
{
    const int zeroBased = line > 0 ? line - 1 : 0;
    ::SendMessageW(hwnd_, SCI_GOTOLINE, zeroBased, 0);
}

int ScintillaEditor::FirstVisibleLine() const
{
    if (hwnd_ == nullptr) {
        return 1;
    }
    const int line = static_cast<int>(::SendMessageW(hwnd_, SCI_GETFIRSTVISIBLELINE, 0, 0));
    return line + 1;
}

void ScintillaEditor::SetFirstVisibleLine(int line)
{
    if (hwnd_ == nullptr) {
        return;
    }
    const int lineCount =
        std::max(1, static_cast<int>(::SendMessageW(hwnd_, SCI_GETLINECOUNT, 0, 0)));
    const int target = std::clamp(line, 1, lineCount);
    ::SendMessageW(hwnd_, SCI_SETFIRSTVISIBLELINE, target - 1, 0);
}

void ScintillaEditor::SetScrollCallback(std::function<void(int firstVisibleLine)> callback)
{
    scrollCallback_ = std::move(callback);
}

void ScintillaEditor::SetReadOnly(bool readOnly)
{
    ::SendMessageW(hwnd_, SCI_SETREADONLY, readOnly ? TRUE : FALSE, 0);
}

void ScintillaEditor::SetDpiScale(double scale)
{
    const double normalized = scale > 0.0 ? scale : 1.0;
    // SetViewSettings() calls this on every layout change; rebuilding all
    // Scintilla styles is expensive and must only happen on a real DPI change.
    if (std::abs(normalized - dpiScale_) < 1e-6) {
        return;
    }
    dpiScale_ = normalized;
    SetupStyles();
}

void ScintillaEditor::SetColorScheme(const theme::EditorScheme& scheme)
{
    scheme_ = scheme;
    if (hwnd_ != nullptr) {
        SetupStyles();
    }
}

void ScintillaEditor::Focus()
{
    if (hwnd_ != nullptr) {
        ::SetFocus(hwnd_);
    }
}

void ScintillaEditor::SetChangeCallback(std::function<void()> callback)
{
    changeCallback_ = std::move(callback);
}

void ScintillaEditor::SetFileDropHandler(
    std::function<void(std::vector<std::wstring>)> handler,
    std::function<bool(const std::wstring&)> isSupported)
{
    fileDropHandler_ = std::move(handler);
    fileDropFilter_ = std::move(isSupported);
    InstallFileDropTarget();
}

void ScintillaEditor::InstallFileDropTarget()
{
    if (hwnd_ == nullptr || fileDropTarget_ != nullptr || !fileDropHandler_) {
        return;
    }

    // Scintilla registers its own IDropTarget in WM_CREATE. Take it over so
    // files dropped on the editor open as tabs, and forward every other drag
    // (text) back to Scintilla so its internal drag and drop still works.
    IDropTarget* original = ui::FileDropTarget::RegisteredTarget(hwnd_);
    fileDropTarget_ = new ui::FileDropTarget(
        [this](std::vector<std::wstring> files) {
            if (fileDropHandler_) {
                fileDropHandler_(std::move(files));
            }
        },
        fileDropFilter_, original);

    if (!ui::FileDropTarget::Register(hwnd_, fileDropTarget_, true)) {
        ReleaseFileDropTarget();
        if (original != nullptr) {
            ::RegisterDragDrop(hwnd_, original);
        }
    }
}

void ScintillaEditor::ReleaseFileDropTarget()
{
    if (fileDropTarget_ == nullptr) {
        return;
    }
    if (hwnd_ != nullptr && ::IsWindow(hwnd_) != FALSE) {
        ui::FileDropTarget::Unregister(hwnd_, fileDropTarget_);
    } else {
        fileDropTarget_->Release();
        fileDropTarget_ = nullptr;
    }
}

int ScintillaEditor::px(int logicalPixels) const
{
    return static_cast<int>(std::lround(static_cast<double>(logicalPixels) * dpiScale_));
}

void ScintillaEditor::SetupStyles()
{
    // Base text style: themed foreground/background with a monospaced face.
    ::SendMessageW(hwnd_, SCI_STYLESETFORE, STYLE_DEFAULT, ToColorRef(scheme_.text));
    ::SendMessageW(hwnd_, SCI_STYLESETBACK, STYLE_DEFAULT,
                   ToColorRef(scheme_.background));
    ::SendMessageW(hwnd_, SCI_STYLESETFONT, STYLE_DEFAULT,
                   reinterpret_cast<LPARAM>(L"Consolas"));
    ::SendMessageW(hwnd_, SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
    ::SendMessageW(hwnd_, SCI_STYLECLEARALL, 0, 0);

    auto setStyle = [this](int style, COLORREF fore, bool bold, bool italic) {
        ::SendMessageW(hwnd_, SCI_STYLESETFORE, style, fore);
        ::SendMessageW(hwnd_, SCI_STYLESETBOLD, style, bold ? TRUE : FALSE);
        ::SendMessageW(hwnd_, SCI_STYLESETITALIC, style, italic ? TRUE : FALSE);
    };

    setStyle(StyleHeading, ToColorRef(scheme_.heading), true, false);
    setStyle(StyleEmphasis, ToColorRef(scheme_.emphasis), false, true);
    setStyle(StyleStrong, ToColorRef(scheme_.strong), true, false);
    setStyle(StyleCode, ToColorRef(scheme_.code), false, false);
    setStyle(StyleCodeBlock, ToColorRef(scheme_.codeBlock), false, false);
    setStyle(StyleLink, ToColorRef(scheme_.link), false, false);
    setStyle(StyleImage, ToColorRef(scheme_.image), false, false);
    setStyle(StyleListMarker, ToColorRef(scheme_.listMarker), true, false);
    setStyle(StyleBlockQuote, ToColorRef(scheme_.blockQuote), false, true);
    setStyle(StyleHtml, ToColorRef(scheme_.html), false, false);
    setStyle(StyleHrule, ToColorRef(scheme_.hrule), false, false);
    setStyle(StyleComment, ToColorRef(scheme_.comment), false, true);
    setStyle(StyleReference, ToColorRef(scheme_.reference), false, false);
    ::SendMessageW(hwnd_, SCI_STYLESETUNDERLINE, StyleLink, TRUE);

    // Line-number gutter.
    ::SendMessageW(hwnd_, SCI_STYLESETFORE, STYLE_LINENUMBER,
                   ToColorRef(scheme_.gutterText));
    ::SendMessageW(hwnd_, SCI_STYLESETBACK, STYLE_LINENUMBER,
                   ToColorRef(scheme_.gutterBg));
    ::SendMessageW(hwnd_, SCI_STYLESETFONT, STYLE_LINENUMBER,
                   reinterpret_cast<LPARAM>(L"Consolas"));
    ::SendMessageW(hwnd_, SCI_STYLESETSIZE, STYLE_LINENUMBER, 9);

    // Margins: line numbers only; no fold/symbol margins.
    ::SendMessageW(hwnd_, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    ::SendMessageW(hwnd_, SCI_SETMARGINWIDTHN, 1, 0);
    ::SendMessageW(hwnd_, SCI_SETMARGINWIDTHN, 2, 0);
    ::SendMessageW(hwnd_, SCI_SETMARGINBACKN, 0, ToColorRef(scheme_.gutterBg));

    ::SendMessageW(hwnd_, SCI_SETCARETLINEVISIBLE, TRUE, 0);
    ::SendMessageW(hwnd_, SCI_SETCARETLINEVISIBLEALWAYS, TRUE, 0);
    ::SendMessageW(hwnd_, SCI_SETCARETLINEBACK, ToColorRef(scheme_.currentLine), 0);
    ::SendMessageW(hwnd_, SCI_SETCARETFORE, ToColorRef(scheme_.caret), 0);
    ::SendMessageW(hwnd_, SCI_SETCARETWIDTH, px(2), 0);
    ::SendMessageW(hwnd_, SCI_SETSELBACK, TRUE, ToColorRef(scheme_.selection));
    ::SendMessageW(hwnd_, SCI_SETSELFORE, TRUE, ToColorRef(scheme_.selectionText));

    ::SendMessageW(hwnd_, SCI_SETTABWIDTH, 4, 0);
    ::SendMessageW(hwnd_, SCI_SETUSETABS, FALSE, 0);
    ::SendMessageW(hwnd_, SCI_SETBUFFEREDDRAW, TRUE, 0);

    UpdateMargins();
}

void ScintillaEditor::UpdateMargins()
{
    ::SendMessageW(hwnd_, SCI_SETMARGINWIDTHN, 1, 0);
    ::SendMessageW(hwnd_, SCI_SETMARGINWIDTHN, 2, 0);
    UpdateLineNumberWidth();
}

void ScintillaEditor::UpdateLineNumberWidth()
{
    if (hwnd_ == nullptr) {
        return;
    }

    const int lines = std::max(1, static_cast<int>(::SendMessageW(hwnd_, SCI_GETLINECOUNT, 0, 0)));
    const std::string sample = std::to_string(lines);
    const int textWidth = static_cast<int>(
        ::SendMessageA(hwnd_, SCI_TEXTWIDTH, STYLE_LINENUMBER,
                       reinterpret_cast<LPARAM>(sample.c_str())));
    const int width = textWidth + px(16);
    if (width != lineNumberWidth_) {
        lineNumberWidth_ = width;
        ::SendMessageW(hwnd_, SCI_SETMARGINWIDTHN, 0, width);
    }
}

void ScintillaEditor::NotifyScrollIfChanged()
{
    const int line = FirstVisibleLine();
    if (line == lastReportedFirstLine_) {
        return;
    }
    lastReportedFirstLine_ = line;

    // Native scrollbars are repainted by Scintilla/Windows on scroll; refresh
    // our themed frame overlay after the position moved.
    if (hwnd_ != nullptr) {
        ::RedrawWindow(hwnd_, nullptr, nullptr,
                       RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
    }
    if (scrollCallback_) {
        scrollCallback_(line);
    }
}

void ScintillaEditor::ApplyStyle(const std::string& source, const std::vector<StyleSpan>& spans)
{
    // Merge overlapping spans into a per-byte semantic style array (engine-
    // independent, unit-tested), then map each byte to a Scintilla style.
    const std::vector<MarkupStyle> merged = MergeStyleSpans(source.size(), spans);
    std::vector<char> styles(merged.size());
    for (std::size_t i = 0; i < merged.size(); ++i) {
        styles[i] = static_cast<char>(StyleIdFor(merged[i]));
    }

    ::SendMessageW(hwnd_, SCI_SETLEXER, SCLEX_CONTAINER, 0);
    ::SendMessageW(hwnd_, SCI_STARTSTYLING, 0, 0x1F);
    ::SendMessageW(hwnd_, SCI_SETSTYLINGEX, static_cast<WPARAM>(styles.size()),
                   reinterpret_cast<LPARAM>(styles.data()));
}

LRESULT CALLBACK ScintillaEditor::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ScintillaEditor* self =
        reinterpret_cast<ScintillaEditor*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCDESTROY && self != nullptr) {
        WNDPROC original = self->originalProc_;
        const LRESULT result = ::CallWindowProcW(original, hwnd, msg, wParam, lParam);
        // Scintilla's Finalise() revokes the window's drop target while it
        // tears the control down, so only the owner's reference is left.
        if (self->fileDropTarget_ != nullptr) {
            self->fileDropTarget_->Release();
            self->fileDropTarget_ = nullptr;
        }
        self->hwnd_ = nullptr;
        self->originalProc_ = nullptr;
        return result;
    }

    return ::CallWindowProcW(self != nullptr ? self->originalProc_ : nullptr, hwnd, msg, wParam,
                             lParam);
}

} // namespace awm
