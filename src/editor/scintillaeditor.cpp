#include "scintillaeditor.h"

#include <algorithm>

#include <scintilla/Scintilla.h>
#include <scintilla/SciLexer.h>

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

} // namespace

ScintillaEditor::ScintillaEditor() = default;

ScintillaEditor::~ScintillaEditor()
{
    if (hwnd_ != nullptr) {
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

void ScintillaEditor::SetText(const std::string& utf8)
{
    ::SendMessageA(hwnd_, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
}

void ScintillaEditor::SetBounds(const RECT& rc)
{
    ::MoveWindow(hwnd_, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

void ScintillaEditor::Show(bool visible)
{
    ::ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
}

std::string ScintillaEditor::GetText() const
{
    const int length = static_cast<int>(::SendMessageA(hwnd_, SCI_GETLENGTH, 0, 0));
    std::string text(static_cast<std::size_t>(length), '\0');
    ::SendMessageA(hwnd_, SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(text.data()));
    return text;
}

void ScintillaEditor::ApplyHighlighting(const std::vector<StyleSpan>& spans)
{
    ApplyStyle(GetText(), spans);
}

void ScintillaEditor::GotoLine(int line)
{
    const int zeroBased = line > 0 ? line - 1 : 0;
    ::SendMessageW(hwnd_, SCI_GOTOLINE, zeroBased, 0);
}

void ScintillaEditor::SetReadOnly(bool readOnly)
{
    ::SendMessageW(hwnd_, SCI_SETREADONLY, readOnly ? TRUE : FALSE, 0);
}

void ScintillaEditor::SetChangeCallback(std::function<void()> callback)
{
    changeCallback_ = std::move(callback);
}

void ScintillaEditor::SetupStyles()
{
    // Base text style: dark on light.
    ::SendMessageW(hwnd_, SCI_STYLESETFORE, STYLE_DEFAULT, 0x202020);
    ::SendMessageW(hwnd_, SCI_STYLESETBACK, STYLE_DEFAULT, 0xFFFFFF);
    ::SendMessageW(hwnd_, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(L"Consolas"));
    ::SendMessageW(hwnd_, SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
    ::SendMessageW(hwnd_, SCI_STYLECLEARALL, 0, 0);

    auto setStyle = [this](int style, int fore, bool bold, bool italic) {
        ::SendMessageW(hwnd_, SCI_STYLESETFORE, style, fore);
        ::SendMessageW(hwnd_, SCI_STYLESETBOLD, style, bold ? TRUE : FALSE);
        ::SendMessageW(hwnd_, SCI_STYLESETITALIC, style, italic ? TRUE : FALSE);
    };

    setStyle(StyleHeading,    0x1A0DAB, true,  false); // blue bold
    setStyle(StyleEmphasis,   0x202020, false, true);  // italic
    setStyle(StyleStrong,     0x000000, true,  false); // bold
    setStyle(StyleCode,       0xC7254E, false, false); // red-ish
    setStyle(StyleCodeBlock,  0x333333, false, false);
    setStyle(StyleLink,       0x0000EE, false, false); // blue
    setStyle(StyleImage,      0x008000, false, false); // green
    setStyle(StyleListMarker, 0x9C6B00, true,  false);
    setStyle(StyleBlockQuote, 0x666666, false, true);
    setStyle(StyleHtml,       0x7A4D00, false, false);
    setStyle(StyleHrule,      0x999999, false, false);
    setStyle(StyleComment,    0x008000, false, true);  // green italic
    setStyle(StyleReference,  0x8A2BE2, false, false); // violet
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

    if (msg == WM_NOTIFY) {
        const auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->code == SCN_MODIFIED && self != nullptr && self->changeCallback_) {
            const auto* scn = reinterpret_cast<SCNotification*>(lParam);
            if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
                self->changeCallback_();
            }
        }
    }

    return ::CallWindowProcW(self != nullptr ? self->originalProc_ : nullptr, hwnd, msg, wParam,
                             lParam);
}

} // namespace awm
