#include "themedialog.h"

#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <initializer_list>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "darkmode.h"
#include "dialogs.h"
#include "filedialogs.h"
#include "markdown/previewhtml.h"
#include "theme/theme_import.h"
#include "theme/theme_json.h"
#include "themeapply.h"
#include "util/stringutil.h"

namespace awm {

namespace {

constexpr const char* kSampleHtml = R"HTML(
<h1>Markdown 主题预览</h1>
<p>这是一段<strong>正文</strong>，包含 <em>斜体</em>、<a href="https://example.com">链接</a> 和 <code>inline code</code>。</p>
<h2>二级标题</h2>
<h3>三级标题</h3>
<blockquote><p>引用文字示例。主题应同时影响引用边框和文字。</p></blockquote>
<ul><li>列表项一</li><li>列表项二</li></ul>
<ol><li>有序列表</li><li>第二项</li></ol>
<table><thead><tr><th>元素</th><th>说明</th></tr></thead>
<tbody><tr><td>标题</td><td>H1–H6</td></tr><tr><td>代码</td><td>Prism token</td></tr></tbody></table>
<pre><code class="language-cpp">#include &lt;string&gt;
int main()
{
    const char* text = "hello";
    return text ? 42 : 0;
}</code></pre>
<pre><code class="language-python">def greet(name):
    return {"name": name, "ok": True}</code></pre>
<hr>
<p>结束。</p>
)HTML";

std::wstring LowerWide(std::wstring text)
{
    for (wchar_t& ch : text) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return text;
}

const wchar_t* FamilyLabel(theme::ThemeFamily family)
{
    switch (family) {
    case theme::ThemeFamily::Neutral:
        return L"中性灰";
    case theme::ThemeFamily::Blue:
        return L"蓝色系";
    case theme::ThemeFamily::Cyan:
        return L"青色系";
    case theme::ThemeFamily::Green:
        return L"绿色系";
    case theme::ThemeFamily::Warm:
        return L"暖色系";
    case theme::ThemeFamily::RedPink:
        return L"红粉系";
    case theme::ThemeFamily::Purple:
        return L"紫罗兰";
    case theme::ThemeFamily::Multicolor:
        return L"多彩";
    }
    return L"中性灰";
}

void SetCue(HWND control, const wchar_t* text)
{
    ::SendMessageW(control, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(text));
}

COLORREF ToColorRef(const theme::ThemeColor& color)
{
    return RGB(color.r, color.g, color.b);
}

/// Selects ClearType fonts into a raw paint DC and restores the previous font
/// on every exit path. Without this the DC uses the stock bitmap system font,
/// which makes the mock window text look grainy.
class FontSelection {
public:
    FontSelection(HDC dc, HFONT font) : dc_(dc)
    {
        if (font != nullptr) {
            old_ = ::SelectObject(dc_, font);
        }
    }

    ~FontSelection()
    {
        if (old_ != nullptr) {
            ::SelectObject(dc_, old_);
        }
    }

    FontSelection(const FontSelection&) = delete;
    FontSelection& operator=(const FontSelection&) = delete;

    void use(HFONT font)
    {
        if (font != nullptr) {
            ::SelectObject(dc_, font);
        }
    }

private:
    HDC dc_ = nullptr;
    HGDIOBJ old_ = nullptr;
};

// The report-view header is a separate SysHeader32 child. After wheel or
// keyboard scrolling the ListView can paint its client over the header without
// repainting the header text. Force a header repaint after every scroll.
LRESULT CALLBACK ThemeListSubclass(HWND hwnd, UINT message, WPARAM wParam,
                                   LPARAM lParam, UINT_PTR id, DWORD_PTR refData)
{
    (void)id;
    (void)refData;
    const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
    switch (message) {
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_KEYDOWN:
        if (HWND header = ListView_GetHeader(hwnd)) {
            ::InvalidateRect(header, nullptr, TRUE);
            ::UpdateWindow(header);
        }
        break;
    default:
        break;
    }
    return result;
}

std::wstring FormatDouble(double value)
{
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%g", value);
    return buffer;
}

std::wstring JoinFirst(const std::vector<std::string>& messages)
{
    std::wstring result;
    for (std::size_t i = 0; i < messages.size() && i < 3; ++i) {
        if (!result.empty()) {
            result += L"\n";
        }
        result += utf8ToWide(messages[i]);
    }
    return result;
}

} // namespace

ThemeDialog::ThemeDialog(std::string currentThemeId, std::filesystem::path themesDirectory,
                         const PreviewAssets* previewAssets, ui::Palette palette,
                         ApplyCallback onApply)
    : palette_(palette),
      currentThemeId_(std::move(currentThemeId)),
      themesDirectory_(std::move(themesDirectory)),
      previewAssets_(previewAssets),
      onApply_(std::move(onApply))
{
    for (COLORREF& color : customColors_) {
        color = RGB(255, 255, 255);
    }
}

ThemeDialog::~ThemeDialog()
{
    if (preview_) {
        preview_->Destroy();
        preview_.reset();
    }
    if (backgroundBrush_ != nullptr) {
        ::DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
    if (surfaceBrush_ != nullptr) {
        ::DeleteObject(surfaceBrush_);
        surfaceBrush_ = nullptr;
    }
}

ThemeDialogResult ThemeDialog::ShowModal(HWND owner, const std::string& currentThemeId,
                                         const std::filesystem::path& themesDirectory,
                                         const PreviewAssets* previewAssets,
                                         const ui::Palette& palette, ApplyCallback onApply)
{
    ThemeDialog dialog(currentThemeId, themesDirectory, previewAssets, palette,
                       std::move(onApply));
    if (!dialog.CreateModalWindow(owner)) {
        return {};
    }

    ::EnableWindow(owner, FALSE);
    dialog.ShowWindow(SW_SHOW);
    dialog.UpdateWindow();
    dialog.RunModal();
    // Re-enable and activate the owner before destroying the (already hidden)
    // dialog so the owner never renders in its disabled state.
    ::EnableWindow(owner, TRUE);
    ::SetForegroundWindow(owner);
    if (dialog.IsWindow()) {
        dialog.ShowWindow(SW_HIDE);
        dialog.DestroyWindow();
    }

    ThemeDialogResult result;
    result.applyRequested = dialog.applyRequested_;
    result.appliedWhileOpen = !dialog.appliedThemeId_.empty();
    result.catalogChanged = dialog.catalogChanged_;
    result.selectedThemeId = result.appliedWhileOpen ? dialog.appliedThemeId_
                                                     : dialog.selectedThemeId_;
    result.savedThemeId = dialog.savedThemeId_;
    return result;
}

bool ThemeDialog::CreateModalWindow(HWND owner)
{
    theme_.initialize(::GetDpiForWindow(owner));
    theme_.setPalette(palette_);
    themeSelectionWidth_ = theme_.px(250);

    int width = theme_.px(1160);
    int height = theme_.px(820);
    RECT work = {};
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int maxWidth = work.right - work.left - theme_.px(24);
    const int maxHeight = work.bottom - work.top - theme_.px(24);
    width = std::min(width, maxWidth);
    height = std::min(height, maxHeight);

    RECT ownerRect = {};
    ::GetWindowRect(owner, &ownerRect);
    int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    x = std::clamp(x, static_cast<int>(work.left) + theme_.px(8),
                   static_cast<int>(work.right) - width - theme_.px(8));
    y = std::clamp(y, static_cast<int>(work.top) + theme_.px(8),
                   static_cast<int>(work.bottom) - height - theme_.px(8));

    RECT rc{x, y, x + width, y + height};
    // Do NOT add WS_EX_COMPOSITED to the whole dialog: it makes the ListView
    // header (a separate SysHeader32 child) repaint continuously. Splitter
    // smoothness is handled by targeted live layout, SWP_NOCOPYBITS and the
    // preview's lightweight SetLiveBounds() path instead.
    if (Create(owner, rc, L"主题设置",
               WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
               WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT) == nullptr) {
        return false;
    }

    CreateChildren();
    Layout();
    return true;
}

void ThemeDialog::CreateChildren()
{
    RECT zero = {0, 0, 0, 0};
    backgroundBrush_ = ::CreateSolidBrush(ui::ToColorRef(palette_.windowBg));
    surfaceBrush_ = ::CreateSolidBrush(ui::ToColorRef(palette_.surface));

    searchEdit_.Create(m_hWnd, zero, L"",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, 0,
                       kSearchId);
    SetCue(searchEdit_.m_hWnd, L"搜索主题名称、作者或来源");

    themeList_.Create(m_hWnd, zero, nullptr,
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_CLIPCHILDREN |
                          LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
                          LVS_NOSORTHEADER,
                      0, kThemeListId);
    themeList_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                        LVS_EX_LABELTIP);
    ::SetWindowTheme(themeList_.m_hWnd, L"Explorer", nullptr);
    ListView_SetBkColor(themeList_.m_hWnd, ui::ToColorRef(palette_.surface));
    ListView_SetTextBkColor(themeList_.m_hWnd, ui::ToColorRef(palette_.surface));
    ListView_SetTextColor(themeList_.m_hWnd, ui::ToColorRef(palette_.textPrimary));
    themeList_.InsertColumn(0, L"名称", LVCFMT_LEFT, theme_.px(135));
    themeList_.InsertColumn(1, L"明暗", LVCFMT_LEFT, theme_.px(45));
    themeList_.InsertColumn(2, L"来源", LVCFMT_LEFT, theme_.px(45));
    if (HWND header = ListView_GetHeader(themeList_.m_hWnd)) {
        ::SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(theme_.uiFont()),
                       TRUE);
    }
    ::SetWindowSubclass(themeList_.m_hWnd, ThemeListSubclass, 1, 0);

    newButton_.Create(m_hWnd, zero, L"新建(&N)",
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, kNewId);
    importButton_.Create(m_hWnd, zero, L"导入(&I)...",
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, kImportId);
    copyButton_.Create(m_hWnd, zero, L"复制(&C)",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, kCopyId);
    deleteButton_.Create(m_hWnd, zero, L"删除(&D)",
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                         kDeleteId);
    exportButton_.Create(m_hWnd, zero, L"导出(&E)...",
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                         kExportId);
    saveButton_.Create(m_hWnd, zero, L"保存(&S)",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0,
                       IDOK);
    applyButton_.Create(m_hWnd, zero, L"立即使用此主题(&U)",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                        kApplyId);
    cancelButton_.Create(m_hWnd, zero, L"关闭(&C)",
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                         IDCANCEL);

    tabs_.Create(m_hWnd, zero, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, kTabsId);
    tabs_.InsertItem(0, L"基本");
    tabs_.InsertItem(1, L"配色");
    tabs_.InsertItem(2, L"正文排版");
    tabs_.InsertItem(3, L"窗口外观");
    tabs_.SetCurSel(0);

    splitter_.Create(m_hWnd, zero, kSplitterId, ui::SplitterAxis::Horizontal);
    splitter_.SetTheme(&theme_);
    if (splitter_.IsWindow()) {
        ui::Splitter::Callbacks callbacks;
        callbacks.onDrag = [this](int y) { PreviewSplitDrag(y); };
        callbacks.onDragEnd = [this](int y) { CommitSplitDrag(y); };
        splitter_.SetCallbacks(std::move(callbacks));
    }

    nameEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                               ES_AUTOHSCROLL | WS_BORDER,
                     0, kNameId);
    SetCue(nameEdit_.m_hWnd, L"主题名称");
    authorEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                 ES_AUTOHSCROLL | WS_BORDER,
                       0, kAuthorId);
    SetCue(authorEdit_.m_hWnd, L"作者（可选）");
    darkCheck_.Create(m_hWnd, zero, L"深色主题",
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, kDarkId);
    familyLabel_.Create(m_hWnd, zero, L"色系：",
                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                        kFamilyLabelId);
    familyCombo_.Create(m_hWnd, zero, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
                            WS_VSCROLL,
                        0, kFamilyId);
    familyCombo_.AddString(L"自动（按强调色）");
    for (int i = 0; i < theme::kThemeFamilyCount; ++i) {
        familyCombo_.AddString(FamilyLabel(static_cast<theme::ThemeFamily>(i)));
    }
    familyCombo_.SetCurSel(0);
    sourceLabel_.Create(m_hWnd, zero, L"",
                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, kSourceId);

    colorList_.Create(m_hWnd, zero, nullptr,
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_VSCROLL |
                          LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY,
                      0, kColorListId);
    editColorButton_.Create(m_hWnd, zero, L"编辑颜色(&E)...",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                            kEditColorId);
    derivedList_.Create(m_hWnd, zero, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_VSCROLL |
                            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY,
                        0, kDerivedListId);

    bodyFontEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                    ES_AUTOHSCROLL | WS_BORDER,
                         0, kBodyFontId);
    SetCue(bodyFontEdit_.m_hWnd, L"如 Georgia, serif");
    codeFontEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                    ES_AUTOHSCROLL | WS_BORDER,
                         0, kCodeFontId);
    SetCue(codeFontEdit_.m_hWnd, L"如 Consolas, monospace");
    bodySizeEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                    ES_AUTOHSCROLL | WS_BORDER,
                         0, kBodySizeId);
    SetCue(bodySizeEdit_.m_hWnd, L"px");
    lineHeightEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                      ES_AUTOHSCROLL | WS_BORDER,
                           0, kLineHeightId);
    SetCue(lineHeightEdit_.m_hWnd, L"倍");
    maxWidthEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                    ES_AUTOHSCROLL | WS_BORDER,
                         0, kMaxWidthId);
    SetCue(maxWidthEdit_.m_hWnd, L"px");
    codeSizeEdit_.Create(m_hWnd, zero, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                                    ES_AUTOHSCROLL | WS_BORDER,
                         0, kCodeSizeId);
    SetCue(codeSizeEdit_.m_hWnd, L"px");
    explainLabel_.Create(
        m_hWnd, zero,
        L"这里只影响 Markdown 的正文排版，不影响窗口、菜单和编辑器字体。",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, kExplainId);
    bodyGroupLabel_.Create(m_hWnd, zero, L"正文",
                           WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                           kBodyGroupId);
    codeGroupLabel_.Create(m_hWnd, zero, L"代码",
                           WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                           kCodeGroupId);
    bodyFontLabel_.Create(m_hWnd, zero, L"正文字体",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                          kBodyFontLabelId);
    bodySizeLabel_.Create(m_hWnd, zero, L"正文字号 (px)",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                          kBodySizeLabelId);
    lineHeightLabel_.Create(m_hWnd, zero, L"行高 (倍)",
                            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                            kLineHeightLabelId);
    maxWidthLabel_.Create(m_hWnd, zero, L"正文最大宽度 (px)",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                          kMaxWidthLabelId);
    codeFontLabel_.Create(m_hWnd, zero, L"等宽字体",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                          kCodeFontLabelId);
    codeSizeLabel_.Create(m_hWnd, zero, L"代码字号 (px)",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                          kCodeSizeLabelId);
    extraCssLabel_.Create(m_hWnd, zero,
                          L"高级：额外 CSS（不允许 @import/url()/括号/@ 规则）",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                          kExtraCssLabelId);
    extraCssEdit_.Create(m_hWnd, zero, L"",
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_MULTILINE |
                             ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
                         0, kExtraCssId);

    hintLabel_.Create(m_hWnd, zero, L"",
                      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, kHintId);
    status_.Create(m_hWnd, zero, L"",
                   WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, 0,
                   kStatusId);

    const auto setFont = [this](CWindow& control, HFONT font) {
        ::SendMessageW(control.m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    };
    for (CWindow* control :
         {static_cast<CWindow*>(&searchEdit_), static_cast<CWindow*>(&nameEdit_),
          static_cast<CWindow*>(&authorEdit_), static_cast<CWindow*>(&darkCheck_),
          static_cast<CWindow*>(&familyCombo_), static_cast<CWindow*>(&bodyFontEdit_),
          static_cast<CWindow*>(&codeFontEdit_), static_cast<CWindow*>(&bodySizeEdit_),
          static_cast<CWindow*>(&lineHeightEdit_), static_cast<CWindow*>(&maxWidthEdit_),
          static_cast<CWindow*>(&codeSizeEdit_), static_cast<CWindow*>(&newButton_),
          static_cast<CWindow*>(&importButton_), static_cast<CWindow*>(&copyButton_),
          static_cast<CWindow*>(&deleteButton_), static_cast<CWindow*>(&exportButton_),
          static_cast<CWindow*>(&saveButton_), static_cast<CWindow*>(&applyButton_),
          static_cast<CWindow*>(&cancelButton_), static_cast<CWindow*>(&tabs_),
          static_cast<CWindow*>(&colorList_), static_cast<CWindow*>(&derivedList_),
          static_cast<CWindow*>(&editColorButton_), static_cast<CWindow*>(&extraCssLabel_)}) {
        setFont(*control, theme_.uiFont());
    }
    setFont(themeList_, theme_.uiFont());
    setFont(extraCssEdit_, theme_.monoFont());
    setFont(hintLabel_, theme_.smallFont());
    setFont(status_, theme_.smallFont());
    setFont(sourceLabel_, theme_.smallFont());
    setFont(familyLabel_, theme_.smallFont());
    setFont(explainLabel_, theme_.smallFont());
    setFont(bodyFontLabel_, theme_.smallFont());
    setFont(bodySizeLabel_, theme_.smallFont());
    setFont(lineHeightLabel_, theme_.smallFont());
    setFont(maxWidthLabel_, theme_.smallFont());
    setFont(codeFontLabel_, theme_.smallFont());
    setFont(codeSizeLabel_, theme_.smallFont());
    setFont(bodyGroupLabel_, theme_.uiFontSemibold());
    setFont(codeGroupLabel_, theme_.uiFontSemibold());

    const auto colorRow = [this](const wchar_t* label, const char* key,
                                 std::function<theme::ThemeColor&(
                                     theme::ThemeSource&)> access) {
        colorRows_.push_back({label, key, std::move(access)});
    };
    colorRow(L"背景", theme::field::kBackground,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.background; });
    colorRow(L"正文", theme::field::kText,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.text; });
    colorRow(L"弱化文字", theme::field::kMuted,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.muted; });
    colorRow(L"强调色", theme::field::kAccent,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.accent; });
    colorRow(L"链接", theme::field::kLink,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.link; });
    colorRow(L"边框", theme::field::kBorder,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.border; });
    colorRow(L"分隔线", theme::field::kHr,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.hr; });
    colorRow(L"粗体", theme::field::kStrong,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.strong; });
    colorRow(L"斜体", theme::field::kEmphasis,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.emphasis; });
    colorRow(L"列表标记", theme::field::kListMarker,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.listMarker; });
    static const wchar_t* kHeadingLabels[6] = {
        L"标题 1", L"标题 2", L"标题 3", L"标题 4", L"标题 5", L"标题 6"};
    for (int i = 0; i < 6; ++i) {
        colorRow(kHeadingLabels[i], theme::field::kHeadings,
                 [i](theme::ThemeSource& s) -> theme::ThemeColor& {
                     return s.headings[i];
                 });
    }
    colorRow(L"引用文字", theme::field::kBlockquoteText,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.blockquoteText;
             });
    colorRow(L"引用边框", theme::field::kBlockquoteBorder,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.blockquoteBorder;
             });
    colorRow(L"代码背景", theme::field::kCodeBackground,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.codeBackground;
             });
    colorRow(L"代码文字", theme::field::kCodeText,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.codeText; });
    colorRow(L"代码边框", theme::field::kCodeBorder,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.codeBorder; });
    colorRow(L"表格边框", theme::field::kTableBorder,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tableBorder;
             });
    colorRow(L"表头背景", theme::field::kTableHeadBackground,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tableHeadBackground;
             });
    colorRow(L"选中背景", theme::field::kSelection,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.selection; });
    colorRow(L"查找匹配", theme::field::kFindMatch,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.findMatch; });
    colorRow(L"查找当前", theme::field::kFindCurrent,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.findCurrent;
             });
    colorRow(L"查找文字", theme::field::kFindText,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.findText; });
    colorRow(L"侧栏背景", theme::field::kSidebarBackground,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.sidebarBackground;
             });
    colorRow(L"侧栏悬停", theme::field::kSidebarHover,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.sidebarHover;
             });
    colorRow(L"危险色", theme::field::kDanger,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.danger; });
    colorRow(L"Token 注释", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.comment;
             });
    colorRow(L"Token 标点", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.punctuation;
             });
    colorRow(L"Token 标签", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.tokens.tag; });
    colorRow(L"Token 属性", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.property;
             });
    colorRow(L"Token 布尔/数字", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.booleanNumber;
             });
    colorRow(L"Token 字符串", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.string;
             });
    colorRow(L"Token 运算符", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.tokens.oper; });
    colorRow(L"Token 函数", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.function;
             });
    colorRow(L"Token 关键字", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& {
                 return s.tokens.keyword;
             });
    colorRow(L"Token 正则", theme::field::kTokens,
             [](theme::ThemeSource& s) -> theme::ThemeColor& { return s.tokens.regex; });

    const auto derived = [this](const wchar_t* label,
                                std::function<theme::ThemeColor(
                                    const theme::ThemeDefinition&)> access) {
        derivedRows_.push_back({label, std::move(access)});
    };
    derived(L"窗口 · 窗口背景", [](const theme::ThemeDefinition& d) {
        return d.chrome.windowBg;
    });
    derived(L"窗口 · 表面", [](const theme::ThemeDefinition& d) {
        return d.chrome.surface;
    });
    derived(L"窗口 · 导航背景", [](const theme::ThemeDefinition& d) {
        return d.chrome.navBg;
    });
    derived(L"窗口 · 标题栏", [](const theme::ThemeDefinition& d) {
        return d.chrome.titleBarBg;
    });
    derived(L"窗口 · 非活动标题栏", [](const theme::ThemeDefinition& d) {
        return d.chrome.titleBarInactive;
    });
    derived(L"窗口 · 标签悬停", [](const theme::ThemeDefinition& d) {
        return d.chrome.tabHover;
    });
    derived(L"窗口 · 按钮悬停", [](const theme::ThemeDefinition& d) {
        return d.chrome.buttonHover;
    });
    derived(L"窗口 · 按钮按下", [](const theme::ThemeDefinition& d) {
        return d.chrome.buttonPressed;
    });
    derived(L"窗口 · 分隔线", [](const theme::ThemeDefinition& d) {
        return d.chrome.divider;
    });
    derived(L"窗口 · 主文字", [](const theme::ThemeDefinition& d) {
        return d.chrome.textPrimary;
    });
    derived(L"窗口 · 次级文字", [](const theme::ThemeDefinition& d) {
        return d.chrome.textSecondary;
    });
    derived(L"窗口 · 强调色", [](const theme::ThemeDefinition& d) {
        return d.chrome.accent;
    });
    derived(L"窗口 · 强调浅底", [](const theme::ThemeDefinition& d) {
        return d.chrome.accentSoft;
    });
    derived(L"窗口 · 行号背景", [](const theme::ThemeDefinition& d) {
        return d.chrome.gutterBg;
    });
    derived(L"窗口 · 当前行", [](const theme::ThemeDefinition& d) {
        return d.chrome.currentLine;
    });
    derived(L"窗口 · 选中", [](const theme::ThemeDefinition& d) {
        return d.chrome.selection;
    });
    derived(L"窗口 · 危险色", [](const theme::ThemeDefinition& d) {
        return d.chrome.danger;
    });
    derived(L"窗口 · 脏标记", [](const theme::ThemeDefinition& d) {
        return d.chrome.dirtyDot;
    });

    derived(L"编辑器 · 背景", [](const theme::ThemeDefinition& d) {
        return d.editor.background;
    });
    derived(L"编辑器 · 正文", [](const theme::ThemeDefinition& d) {
        return d.editor.text;
    });
    derived(L"编辑器 · 标题", [](const theme::ThemeDefinition& d) {
        return d.editor.heading;
    });
    derived(L"编辑器 · 强调", [](const theme::ThemeDefinition& d) {
        return d.editor.emphasis;
    });
    derived(L"编辑器 · 粗体", [](const theme::ThemeDefinition& d) {
        return d.editor.strong;
    });
    derived(L"编辑器 · 行内代码", [](const theme::ThemeDefinition& d) {
        return d.editor.code;
    });
    derived(L"编辑器 · 代码块", [](const theme::ThemeDefinition& d) {
        return d.editor.codeBlock;
    });
    derived(L"编辑器 · 链接", [](const theme::ThemeDefinition& d) {
        return d.editor.link;
    });
    derived(L"编辑器 · 图片", [](const theme::ThemeDefinition& d) {
        return d.editor.image;
    });
    derived(L"编辑器 · 列表标记", [](const theme::ThemeDefinition& d) {
        return d.editor.listMarker;
    });
    derived(L"编辑器 · 引用", [](const theme::ThemeDefinition& d) {
        return d.editor.blockQuote;
    });
    derived(L"编辑器 · HTML", [](const theme::ThemeDefinition& d) {
        return d.editor.html;
    });
    derived(L"编辑器 · 分隔线", [](const theme::ThemeDefinition& d) {
        return d.editor.hrule;
    });
    derived(L"编辑器 · 注释", [](const theme::ThemeDefinition& d) {
        return d.editor.comment;
    });
    derived(L"编辑器 · 引用标记", [](const theme::ThemeDefinition& d) {
        return d.editor.reference;
    });
    derived(L"编辑器 · 行号背景", [](const theme::ThemeDefinition& d) {
        return d.editor.gutterBg;
    });
    derived(L"编辑器 · 行号文字", [](const theme::ThemeDefinition& d) {
        return d.editor.gutterText;
    });
    derived(L"编辑器 · 当前行", [](const theme::ThemeDefinition& d) {
        return d.editor.currentLine;
    });
    derived(L"编辑器 · 选中背景", [](const theme::ThemeDefinition& d) {
        return d.editor.selection;
    });
    derived(L"编辑器 · 选中文字", [](const theme::ThemeDefinition& d) {
        return d.editor.selectionText;
    });
    derived(L"编辑器 · 光标", [](const theme::ThemeDefinition& d) {
        return d.editor.caret;
    });

    derived(L"滚动条 · 轨道", [](const theme::ThemeDefinition& d) {
        return d.preview.pageBg;
    });
    derived(L"滚动条 · 滑块", [](const theme::ThemeDefinition& d) {
        return theme::Blend(d.preview.pageBg, d.preview.pageText, 0.28);
    });
    derived(L"滚动条 · 箭头", [](const theme::ThemeDefinition& d) {
        return d.chrome.textSecondary;
    });

    for (std::size_t i = 0; i < derivedRows_.size(); ++i) {
        const int item = derivedList_.AddString(L"");
        derivedList_.SetItemData(item, static_cast<DWORD_PTR>(i));
    }

    preview_ = CreatePreviewView(PreviewEngine::Ie);
    if (preview_) {
        PreviewCallbacks callbacks;
        callbacks.onLink = [](const std::string&) {};
        preview_->SetCallbacks(std::move(callbacks));
        RECT previewRect{0, 0, 1, 1};
        preview_->Create(m_hWnd, previewRect);
        preview_->Show(true);
    }

    ApplyThemeToDialog();
    ReloadStore();

    int initial = theme::IndexOfTheme(currentThemeId_);
    if (initial < 0 && !theme::AllThemes().empty()) {
        initial = 0;
    }
    if (initial >= 0) {
        selectedThemeId_ = theme::AllThemes()[static_cast<std::size_t>(initial)].id;
        RefreshThemeList();
        LoadThemeIntoEditor(initial);
    }
    ::SetFocus(themeList_.m_hWnd);
}

void ThemeDialog::ApplyThemeToDialog()
{
    const ColorMode mode =
        IsDarkPalette(palette_) ? ColorMode::Dark : ColorMode::Light;
    ApplyColorModeToWindow(m_hWnd, mode);
    ApplyColorModeToWindow(themeList_.m_hWnd, mode);
    ApplyColorModeToWindow(colorList_.m_hWnd, mode);
    ApplyColorModeToWindow(tabs_.m_hWnd, mode);
    ui::SetThemedScrollbar(themeList_.m_hWnd, MakeScrollbarColors(palette_));
    ui::SetThemedScrollbar(colorList_.m_hWnd, MakeScrollbarColors(palette_));
    ui::SetThemedScrollbar(extraCssEdit_.m_hWnd, MakeScrollbarColors(palette_));
    splitter_.SetTheme(&theme_);
    Invalidate(TRUE);
}

void ThemeDialog::ReloadStore()
{
    theme::ThemeStoreLoadResult loaded = theme::LoadThemeStore(themesDirectory_);
    userThemes_ = std::move(loaded.themes);
    theme::SetUserThemes(theme::DefinitionsOf(userThemes_));
    if (!loaded.errors.empty()) {
        SetStatus(L"有 " + std::to_wstring(loaded.errors.size()) +
                  L" 个主题文件无法加载：" + utf8ToWide(loaded.errors.front()));
    }
    RefreshThemeList();
}

void ThemeDialog::RefreshThemeList()
{
    if (!themeList_.IsWindow()) {
        return;
    }
    const std::vector<theme::ThemeDefinition>& all = theme::AllThemes();
    std::string wanted = selectedThemeId_;
    if (wanted.empty() && currentIndex_ >= 0 &&
        static_cast<std::size_t>(currentIndex_) < all.size()) {
        wanted = all[static_cast<std::size_t>(currentIndex_)].id;
    }

    const std::wstring filter = LowerWide(TextOf(searchEdit_));
    const bool autoLoad = filter.empty();
    suppressSelection_ = true;
    themeList_.DeleteAllItems();

    int targetItem = -1;
    int insertIndex = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const theme::ThemeDefinition& definition = all[i];
        const std::wstring name = utf8ToWide(definition.displayName);
        const std::wstring author = utf8ToWide(definition.author);
        const std::wstring source = definition.userDefined ? L"自定义" : L"内置";
        if (!filter.empty()) {
            const std::wstring haystack = LowerWide(name + L" " + author + L" " + source);
            if (haystack.find(filter) == std::wstring::npos) {
                continue;
            }
        }
        const int item = themeList_.InsertItem(insertIndex, name.c_str());
        themeList_.SetItemText(item, 1, definition.dark ? L"深色" : L"浅色");
        themeList_.SetItemText(item, 2, source.c_str());
        themeList_.SetItemData(item, static_cast<DWORD_PTR>(i));
        if (!wanted.empty() && definition.id == wanted) {
            targetItem = item;
        }
        ++insertIndex;
    }

    if (targetItem < 0 && autoLoad && themeList_.GetItemCount() > 0) {
        targetItem = 0;
    }
    if (targetItem >= 0) {
        themeList_.SetItemState(targetItem, LVIS_SELECTED | LVIS_FOCUSED,
                                LVIS_SELECTED | LVIS_FOCUSED);
        themeList_.EnsureVisible(targetItem, FALSE);
    }
    suppressSelection_ = false;

    if (autoLoad && targetItem >= 0) {
        const int catalogIndex =
            static_cast<int>(themeList_.GetItemData(targetItem));
        if (catalogIndex != currentIndex_) {
            LoadThemeIntoEditor(catalogIndex);
        }
    }
}

void ThemeDialog::LoadThemeIntoEditor(int catalogIndex)
{
    const std::vector<theme::ThemeDefinition>& all = theme::AllThemes();
    if (catalogIndex < 0 || static_cast<std::size_t>(catalogIndex) >= all.size()) {
        return;
    }
    const theme::ThemeDefinition& definition = all[static_cast<std::size_t>(catalogIndex)];
    currentIndex_ = catalogIndex;
    isBuiltin_ = !definition.userDefined;
    selectedThemeId_ = definition.id;

    bool loaded = false;
    if (definition.userDefined) {
        for (const theme::StoredTheme& stored : userThemes_) {
            if (stored.definition.id == definition.id) {
                draft_ = stored.source;
                loaded = true;
                break;
            }
        }
    }
    if (!loaded) {
        draft_ = theme::SourceFromDefinition(definition, false);
    }

    dirty_ = false;
    sessionEditedFields_.clear();
    suppressFieldChanges_ = true;
    FillControlsFromDraft();
    suppressFieldChanges_ = false;
    UpdateEditorEnabledState();
    UpdateHint();
    RefreshPreviewDefinition();
    RenderPreview();
    Invalidate();
}

void ThemeDialog::FillControlsFromDraft()
{
    SetText(nameEdit_, draft_.name);
    SetText(authorEdit_, draft_.author);
    darkCheck_.SetCheck(draft_.dark ? BST_CHECKED : BST_UNCHECKED);
    familyCombo_.SetCurSel(draft_.familyExplicit
                               ? static_cast<int>(draft_.family) + 1
                               : 0);

    SetText(bodyFontEdit_, draft_.typography.bodyFont);
    SetText(codeFontEdit_, draft_.typography.codeFont);
    bodySizeEdit_.SetWindowTextW(std::to_wstring(draft_.typography.bodySize).c_str());
    lineHeightEdit_.SetWindowTextW(FormatDouble(draft_.typography.lineHeight).c_str());
    maxWidthEdit_.SetWindowTextW(std::to_wstring(draft_.typography.maxWidth).c_str());
    codeSizeEdit_.SetWindowTextW(std::to_wstring(draft_.typography.codeSize).c_str());
    SetText(extraCssEdit_, draft_.extraCss);

    std::wstring source = L"来源：";
    source += draft_.source.empty() ? L"内置主题" : utf8ToWide(draft_.source);
    if (!draft_.license.empty()) {
        source += L"    许可证：" + utf8ToWide(draft_.license);
    }
    sourceLabel_.SetWindowTextW(source.c_str());

    colorList_.ResetContent();
    for (std::size_t i = 0; i < colorRows_.size(); ++i) {
        const int item = colorList_.AddString(L"");
        colorList_.SetItemData(item, static_cast<DWORD_PTR>(i));
    }
    colorList_.Invalidate();
}

void ThemeDialog::ReadControlsIntoDraft()
{
    draft_.name = Utf8TextOf(nameEdit_);
    draft_.author = Utf8TextOf(authorEdit_);
    draft_.dark = darkCheck_.GetCheck() == BST_CHECKED;
    draft_.darkExplicit = true;

    const int family = familyCombo_.GetCurSel();
    if (family > 0 && family <= theme::kThemeFamilyCount) {
        draft_.family = static_cast<theme::ThemeFamily>(family - 1);
        draft_.familyExplicit = true;
    } else {
        draft_.family = theme::DeriveFamily(draft_.accent);
        draft_.familyExplicit = false;
    }

    draft_.typography.bodyFont = Utf8TextOf(bodyFontEdit_);
    draft_.typography.codeFont = Utf8TextOf(codeFontEdit_);
    const int bodySize = _wtoi(TextOf(bodySizeEdit_).c_str());
    if (bodySize > 0) {
        draft_.typography.bodySize = bodySize;
    }
    const double lineHeight = _wtof(TextOf(lineHeightEdit_).c_str());
    if (lineHeight > 0.0) {
        draft_.typography.lineHeight = lineHeight;
    }
    const int maxWidth = _wtoi(TextOf(maxWidthEdit_).c_str());
    if (maxWidth > 0) {
        draft_.typography.maxWidth = maxWidth;
    }
    const int codeSize = _wtoi(TextOf(codeSizeEdit_).c_str());
    if (codeSize > 0) {
        draft_.typography.codeSize = codeSize;
    }
    draft_.extraCss = Utf8TextOf(extraCssEdit_);
}

void ThemeDialog::UpdateEditorEnabledState()
{
    const BOOL editable = isBuiltin_ ? FALSE : TRUE;
    CWindow* controls[] = {&nameEdit_,     &authorEdit_,   &darkCheck_,
                           &familyCombo_,  &editColorButton_, &bodyFontEdit_,
                           &codeFontEdit_, &bodySizeEdit_,  &lineHeightEdit_,
                           &maxWidthEdit_, &codeSizeEdit_,  &extraCssEdit_};
    for (CWindow* control : controls) {
        ::EnableWindow(control->m_hWnd, editable);
    }
    // A disabled list box cannot scroll. Keep both color tables enabled even
    // for read-only built-ins; editing is blocked by EditSelectedColor().
    ::EnableWindow(colorList_.m_hWnd, TRUE);
    ::EnableWindow(derivedList_.m_hWnd, TRUE);
    ::EnableWindow(deleteButton_.m_hWnd,
                   (!isBuiltin_ && !selectedThemeId_.empty()) ? TRUE : FALSE);
    saveButton_.SetWindowTextW(L"保存(&S)");
    ::EnableWindow(saveButton_.m_hWnd, editable);
    applyButton_.SetWindowTextW(L"立即使用此主题(&U)");
    ::EnableWindow(applyButton_.m_hWnd, TRUE);
}

void ThemeDialog::UpdateHint()
{
    if (tabs_.IsWindow() && tabs_.GetCurSel() == 3) {
        hintLabel_.SetWindowTextW(
            L"窗口外壳、编辑器和滚动条颜色由 Markdown 配色自动派生，只读；"
            L"编辑“配色/排版”后会更新。");
        return;
    }

    std::wstring hint;
    if (isBuiltin_) {
        hint = L"内置主题只读，可浏览配色与派生色；点“立即使用此主题”切换，窗口不会关闭。";
    } else if (currentIndex_ < 0) {
        hint = L"新主题：“保存”写入文件并保持窗口打开；“立即使用此主题”保存并立即应用。";
    } else {
        hint = L"“保存”写入文件并保持窗口打开；“立即使用此主题”保存并立即应用；"
               L"“关闭”结束。";
    }
    hintLabel_.SetWindowTextW(hint.c_str());
}

void ThemeDialog::RefreshPreviewDefinition()
{
    previewDefinition_ = theme::BuildDefinition(draft_, true);
    if (derivedList_.IsWindow()) {
        derivedList_.Invalidate();
    }
    if (chromeMockRect_.right > chromeMockRect_.left) {
        ::InvalidateRect(m_hWnd, &chromeMockRect_, FALSE);
    }
}

void ThemeDialog::RenderPreview()
{
    if (!preview_) {
        return;
    }
    RefreshPreviewDefinition();
    PreviewHtmlOptions options;
    options.theme = &previewDefinition_;
    if (previewAssets_ != nullptr) {
        options.inlineScript = previewAssets_->prismScript;
    }
    const std::string html = BuildPreviewHtml(kSampleHtml, options);
    preview_->SetHtml(html, "");
}

void ThemeDialog::SchedulePreview()
{
    ::SetTimer(m_hWnd, kPreviewTimerId, kPreviewDelayMs, nullptr);
}

void ThemeDialog::CreateNewTheme(bool duplicate)
{
    theme::ThemeSource base = draft_;
    if (currentIndex_ >= 0 &&
        static_cast<std::size_t>(currentIndex_) < theme::AllThemes().size()) {
        const theme::ThemeDefinition& definition =
            theme::AllThemes()[static_cast<std::size_t>(currentIndex_)];
        if (isBuiltin_) {
            base = theme::SourceFromDefinition(definition, false);
        }
    }
    base.id.clear();
    base.name = duplicate ? base.name + " 副本" : "新建主题";
    base.source.clear();
    base.license.clear();

    draft_ = std::move(base);
    draft_.id.clear();
    isBuiltin_ = false;
    currentIndex_ = -1;
    selectedThemeId_.clear();
    sessionEditedFields_.clear();
    dirty_ = true;

    suppressFieldChanges_ = true;
    FillControlsFromDraft();
    suppressFieldChanges_ = false;
    UpdateEditorEnabledState();
    UpdateHint();
    RefreshPreviewDefinition();
    RenderPreview();

    suppressSelection_ = true;
    themeList_.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    suppressSelection_ = false;
    Invalidate();
}

void ThemeDialog::ImportThemes()
{
    const std::vector<std::wstring> paths = ShowOpenThemeDialog(m_hWnd);
    if (paths.empty()) {
        return;
    }

    int importedCount = 0;
    std::string firstId;
    std::vector<std::string> problems;
    std::set<std::string> usedNames;
    for (const theme::ThemeDefinition& definition : theme::AllThemes()) {
        usedNames.insert(definition.displayName);
    }

    for (const std::wstring& widePath : paths) {
        const std::filesystem::path path(widePath);
        theme::ImportResult imported = theme::ImportThemeFile(path);
        if (!imported.error.empty()) {
            problems.push_back(imported.error);
            continue;
        }
        for (theme::ImportedTheme& importedTheme : imported.themes) {
            theme::ThemeSource source = std::move(importedTheme.source);
            const std::string baseName =
                source.name.empty() ? path.stem().string() : source.name;
            std::string name = baseName;
            int suffix = 2;
            while (usedNames.count(name) != 0) {
                name = baseName + " (" + std::to_string(suffix++) + ")";
            }
            usedNames.insert(name);
            source.name = name;

            std::string error;
            if (!theme::SaveThemeToStore(themesDirectory_, source, error, false)) {
                problems.push_back(error);
                continue;
            }
            ++importedCount;
            if (firstId.empty()) {
                firstId = source.id;
            }
            for (const std::string& warning : importedTheme.warnings) {
                problems.push_back(warning);
            }
        }
        for (const std::string& warning : imported.warnings) {
            problems.push_back(warning);
        }
    }

    if (!firstId.empty()) {
        catalogChanged_ = true;
        selectedThemeId_ = firstId;
        searchEdit_.SetWindowTextW(L"");
        ReloadStore();
        selectedThemeId_ = firstId;
    }
    std::wstring status =
        L"已导入 " + std::to_wstring(importedCount) + L" 个主题。";
    if (!problems.empty()) {
        status += L" ";
        status += JoinFirst(problems);
    }
    SetStatus(status);
    if (importedCount == 0 && !problems.empty()) {
        ShowError(m_hWnd, JoinFirst(problems));
    }
}

void ThemeDialog::DeleteCurrentTheme()
{
    if (isBuiltin_ || selectedThemeId_.empty()) {
        return;
    }
    if (::MessageBoxW(m_hWnd, L"确定删除该主题？此操作不可撤销。", L"删除主题",
                      MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    std::string error;
    if (!theme::DeleteThemeFromStore(themesDirectory_, selectedThemeId_, error)) {
        ShowError(m_hWnd, utf8ToWide(error));
        return;
    }
    catalogChanged_ = true;
    dirty_ = false;
    selectedThemeId_.clear();
    currentIndex_ = -1;
    ReloadStore();
    SetStatus(L"主题已删除。");
}

void ThemeDialog::ExportCurrentTheme()
{
    theme::ThemeSource source = draft_;
    if (isBuiltin_ && currentIndex_ >= 0 &&
        static_cast<std::size_t>(currentIndex_) < theme::AllThemes().size()) {
        source = theme::SourceFromDefinition(
            theme::AllThemes()[static_cast<std::size_t>(currentIndex_)], true);
    }
    const std::wstring suggested =
        utf8ToWide(theme::ThemeSlug(source.name)) + L".awmtheme.json";
    const std::wstring path = ShowSaveThemeDialog(m_hWnd, suggested);
    if (path.empty()) {
        return;
    }
    std::string error;
    if (!theme::SaveThemeSourceFile(std::filesystem::path(path), source, error)) {
        ShowError(m_hWnd, utf8ToWide(error));
        return;
    }
    SetStatus(L"主题已导出。");
}

void ThemeDialog::EditSelectedColor()
{
    if (isBuiltin_) {
        return;
    }
    const int selection = colorList_.GetCurSel();
    if (selection < 0) {
        return;
    }
    const int row = static_cast<int>(colorList_.GetItemData(selection));
    if (row < 0 || static_cast<std::size_t>(row) >= colorRows_.size()) {
        return;
    }
    const ColorRow& rowInfo = colorRows_[static_cast<std::size_t>(row)];
    const theme::ThemeColor current = rowInfo.access(draft_);
    CHOOSECOLORW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = m_hWnd;
    dialog.rgbResult = RGB(current.r, current.g, current.b);
    dialog.lpCustColors = customColors_;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (::ChooseColorW(&dialog) == FALSE) {
        return;
    }
    theme::ThemeColor edited = current;
    edited.r = GetRValue(dialog.rgbResult);
    edited.g = GetGValue(dialog.rgbResult);
    edited.b = GetBValue(dialog.rgbResult);
    rowInfo.access(draft_) = edited;
    draft_.explicitFields.insert(rowInfo.key);
    sessionEditedFields_.insert(rowInfo.key);
    // Changing a base color re-derives the structural colors around it
    // (sidebar/window chrome, code blocks, borders, quotes, ...) unless the
    // user set one of them by hand in this session. Colors that give the
    // theme its identity (headings, tokens, emphasis, ...) stay untouched.
    const auto releaseDerived = [this](std::initializer_list<const char*> fields) {
        for (const char* field : fields) {
            if (sessionEditedFields_.find(field) == sessionEditedFields_.end()) {
                draft_.explicitFields.erase(field);
            }
        }
    };
    const std::string_view editedKey = rowInfo.key;
    if (editedKey == theme::field::kBackground || editedKey == theme::field::kText) {
        releaseDerived({theme::field::kMuted,
                        theme::field::kBorder,
                        theme::field::kHr,
                        theme::field::kBlockquoteText,
                        theme::field::kBlockquoteBorder,
                        theme::field::kCodeBackground,
                        theme::field::kCodeText,
                        theme::field::kCodeBorder,
                        theme::field::kTableBorder,
                        theme::field::kTableHeadBackground,
                        theme::field::kSidebarBackground,
                        theme::field::kSidebarHover,
                        theme::field::kFindText});
    } else if (editedKey == theme::field::kAccent) {
        releaseDerived({theme::field::kSelection,
                        theme::field::kFindMatch,
                        theme::field::kFindCurrent});
    }
    draft_ = theme::RefreshDerivedFields(draft_);
    // Dark/inferred metadata may have changed with the base colors.
    darkCheck_.SetCheck(draft_.dark ? BST_CHECKED : BST_UNCHECKED);
    dirty_ = true;
    colorList_.Invalidate();
    UpdateHint();
    RefreshPreviewDefinition();
    SchedulePreview();
    Invalidate();
}

void ThemeDialog::SelectThemeId(const std::string& id)
{
    selectedThemeId_ = id;
    RefreshThemeList();
}

void ThemeDialog::SetStatus(const std::wstring& text)
{
    std::wstring singleLine;
    singleLine.reserve(std::min<std::size_t>(text.size(), 260));
    for (const wchar_t ch : text) {
        if (singleLine.size() >= 260) {
            break;
        }
        singleLine.push_back((ch == L'\n' || ch == L'\r') ? L' ' : ch);
    }
    status_.SetWindowTextW(singleLine.c_str());
    Invalidate(FALSE);
}

void ThemeDialog::OnFieldEdited()
{
    if (suppressFieldChanges_) {
        return;
    }
    ReadControlsIntoDraft();
    dirty_ = true;
    UpdateHint();
    RefreshPreviewDefinition();
    SchedulePreview();
    Invalidate();
}

void ThemeDialog::OnOk()
{
    if (isBuiltin_) {
        // Nothing to save for a read-only built-in theme. "保存" is disabled
        // in that state; if it is invoked anyway, keep browsing.
        SetStatus(L"内置主题只读；可点“立即使用此主题”切换，或“复制”后再编辑。");
        return;
    }

    std::string savedId;
    if (!SaveCurrentTheme(savedId)) {
        return;
    }
    savedThemeId_ = savedId;
    selectedThemeId_ = savedId;
    // Keep the dialog open so the user can keep tuning after saving.
    SetStatus(L"已保存主题“" + utf8ToWide(draft_.name) + L"”。");
}

void ThemeDialog::OnApply()
{
    std::string themeId = draft_.id;
    if (!isBuiltin_) {
        std::string savedId;
        if (!SaveCurrentTheme(savedId)) {
            return;
        }
        savedThemeId_ = savedId;
        themeId = savedId;
    }
    selectedThemeId_ = themeId;
    appliedThemeId_ = themeId;
    applyRequested_ = true;
    // The owner applies the theme while this dialog stays open, so the live
    // window and the dialog preview never disagree.
    if (onApply_) {
        onApply_(themeId);
    }
    SetStatus(L"已应用此主题；可继续调整，“关闭”结束。");
}

bool ThemeDialog::SaveCurrentTheme(std::string& savedId)
{
    ReadControlsIntoDraft();
    if (draft_.name.empty()) {
        ::MessageBoxW(m_hWnd, L"主题名称不能为空。", L"主题设置",
                      MB_OK | MB_ICONWARNING);
        return false;
    }

    std::vector<std::string> warnings;
    const std::string sanitizedCss = theme::SanitizeThemeCss(draft_.extraCss, warnings);
    if (!warnings.empty()) {
        const std::wstring message =
            L"额外 CSS 含不允许的片段，将被忽略：\n" + JoinFirst(warnings);
        if (::MessageBoxW(m_hWnd, message.c_str(), L"主题设置",
                          MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            return false;
        }
    }
    draft_.extraCss = sanitizedCss;

    theme::ThemeSource source = draft_;
    std::string error;
    const bool overwriteExisting = currentIndex_ >= 0 && !isBuiltin_;
    if (!theme::SaveThemeToStore(themesDirectory_, source, error, overwriteExisting)) {
        ShowError(m_hWnd, utf8ToWide(error));
        return false;
    }

    savedId = source.id;
    draft_ = source;
    selectedThemeId_ = savedId;
    catalogChanged_ = true;
    dirty_ = false;
    // Clear the search so the saved theme is visible after the catalog reload.
    searchEdit_.SetWindowTextW(L"");
    ReloadStore();
    selectedThemeId_ = savedId;
    return true;
}

std::wstring ThemeDialog::TextOf(CWindow& control) const
{
    if (!control.IsWindow()) {
        return {};
    }
    const int length = control.GetWindowTextLengthW();
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    control.GetWindowTextW(&text[0], length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::string ThemeDialog::Utf8TextOf(CWindow& control) const
{
    return wideToUtf8(TextOf(control));
}

void ThemeDialog::SetText(CWindow& control, const std::string& utf8)
{
    control.SetWindowTextW(utf8ToWide(utf8).c_str());
}

int ThemeDialog::MinUpperHeight() const
{
    switch (tabs_.GetCurSel()) {
    case 2: // 正文排版: explanation, labels, fields and extra CSS
        return theme_.px(320);
    case 3: // 窗口外观: mock window and color list
        return theme_.px(230);
    default:
        return theme_.px(160);
    }
}

int ThemeDialog::ClampSplitTop(int y, int* contentTopOut, int* spanOut) const
{
    RECT client = {};
    GetClientRect(&client);
    const int height = client.bottom - client.top;
    const int margin = theme_.px(12);
    const int gap = theme_.px(8);
    const int buttonHeight = theme_.px(26);
    const int statusY = height - margin - buttonHeight;
    const int contentTop = margin + theme_.px(26) + gap;
    const int contentBottom = statusY - gap;
    const int splitterHeight = theme_.px(8);
    const int span = contentBottom - contentTop - splitterHeight;
    if (contentTopOut != nullptr) {
        *contentTopOut = contentTop;
    }
    if (spanOut != nullptr) {
        *spanOut = span;
    }
    if (span <= theme_.px(120)) {
        return -1;
    }

    const int minUpper = MinUpperHeight();
    const int minLower = gap + theme_.px(180);
    const int maxUpper = std::max(minUpper, span - minLower);
    return std::clamp(y - splitterHeight / 2, contentTop + minUpper,
                      contentTop + maxUpper);
}

void ThemeDialog::PreviewSplitDrag(int y)
{
    int contentTop = 0;
    int span = 0;
    const int top = ClampSplitTop(y, &contentTop, &span);
    if (top < 0 || span <= 0) {
        return;
    }
    upperRatio_ = static_cast<double>(top - contentTop) / span;
    LayoutSplitter();
}

void ThemeDialog::CommitSplitDrag(int y)
{
    int contentTop = 0;
    int span = 0;
    const int top = ClampSplitTop(y, &contentTop, &span);
    if (top < 0 || span <= 0) {
        return;
    }
    upperRatio_ = static_cast<double>(top - contentTop) / span;
    // Full layout only once, on release; the live path above moves only the
    // controls affected by the splitter.
    Layout();
}

void ThemeDialog::LayoutSplitter()
{
    if (!IsWindow()) {
        return;
    }
    RECT client = {};
    GetClientRect(&client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = theme_.px(12);
    const int gap = theme_.px(8);
    const int buttonHeight = theme_.px(26);
    const int statusY = height - margin - buttonHeight;
    const int leftWidth = themeSelectionWidth_;
    const int rightX = margin + leftWidth + gap;
    const int rightWidth = width - rightX - margin;
    const int pageTop = margin + theme_.px(26) + gap;
    const int splitterHeight = theme_.px(8);
    const int contentBottom = statusY - gap;
    const int span = contentBottom - pageTop - splitterHeight;
    const int minUpper = MinUpperHeight();
    const int minLower = gap + theme_.px(180);
    const int maxUpper = std::max(minUpper, span - minLower);
    const int upperHeight =
        std::clamp(static_cast<int>(span * upperRatio_), minUpper, maxUpper);
    if (span > 0) {
        upperRatio_ = static_cast<double>(upperHeight) / span;
    }
    const int splitterTop = pageTop + upperHeight;
    const int pageBottom = splitterTop - gap;
    const int previewTop = splitterTop + splitterHeight + gap;

    pageRect_ = {rightX, pageTop, rightX + rightWidth, pageBottom};
    splitterRect_ = {rightX, splitterTop, rightX + rightWidth,
                     splitterTop + splitterHeight};
    previewRect_ = {rightX, previewTop, rightX + rightWidth, contentBottom};
    chromeMockRect_ = {};

    const UINT flags = SWP_NOZORDER | SWP_NOCOPYBITS | SWP_NOACTIVATE;
    ::SetWindowPos(splitter_.m_hWnd, nullptr, splitterRect_.left,
                   splitterRect_.top, splitterRect_.right - splitterRect_.left,
                   splitterRect_.bottom - splitterRect_.top, flags);

    const int pageX = pageRect_.left + theme_.px(8);
    const int pageWidth = pageRect_.right - pageRect_.left - theme_.px(16);
    const int hintY = pageRect_.bottom - theme_.px(22);
    const int pageListTop = pageRect_.top + theme_.px(8);
    const int page = tabs_.GetCurSel();
    if (page == 1) {
        const int editColorY = hintY - gap - theme_.px(24);
        ::SetWindowPos(colorList_.m_hWnd, nullptr, pageX, pageListTop, pageWidth,
                       std::max(theme_.px(60), editColorY - gap - pageListTop),
                       flags);
        ::SetWindowPos(editColorButton_.m_hWnd, nullptr, pageX, editColorY,
                       theme_.px(140), theme_.px(24), flags);
    } else if (page == 2) {
        // Only the extra-CSS editor depends on the page bottom; the labels and
        // fields above it are anchored to the page top.
        int typeY = pageRect_.top + theme_.px(8);
        typeY += theme_.px(18) + theme_.px(6);  // explanation
        typeY += theme_.px(18) + theme_.px(2);  // body group
        typeY += theme_.px(28);                 // body font
        typeY += theme_.px(28) + gap;           // numeric row + gap
        typeY += theme_.px(18) + theme_.px(2);  // code group
        typeY += theme_.px(28);                 // code font
        typeY += theme_.px(28) + gap;           // code size + gap
        typeY += theme_.px(20) + theme_.px(4);  // advanced label
        ::SetWindowPos(extraCssEdit_.m_hWnd, nullptr, pageX, typeY, pageWidth,
                       std::max(theme_.px(60),
                                pageBottom - typeY - theme_.px(28)),
                       flags);
    } else if (page == 3) {
        const int mockHeight = theme_.px(112);
        chromeMockRect_ = {pageX, pageListTop, pageX + pageWidth,
                           pageListTop + mockHeight};
        const int derivedTop = chromeMockRect_.bottom + gap;
        ::SetWindowPos(derivedList_.m_hWnd, nullptr, pageX, derivedTop, pageWidth,
                       std::max(theme_.px(60), hintY - gap - derivedTop), flags);
    }

    ::SetWindowPos(hintLabel_.m_hWnd, nullptr, pageX, hintY, pageWidth,
                   theme_.px(20), flags);

    if (preview_) {
        preview_->SetLiveBounds(previewRect_);
    }
    // Repaint only the right panel. Invalidating the whole dialog also touches
    // the area behind the left ListView, which can make its header flicker.
    RECT dirty{pageRect_.left - gap, pageRect_.top - gap, pageRect_.right + gap,
               previewRect_.bottom + gap};
    ::InvalidateRect(m_hWnd, &dirty, FALSE);
}

void ThemeDialog::Layout()
{
    if (!IsWindow() || !themeList_.IsWindow()) {
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = theme_.px(12);
    const int gap = theme_.px(8);
    const int buttonHeight = theme_.px(26);
    const int leftWidth = themeSelectionWidth_;

    searchEdit_.SetWindowPos(nullptr, margin, margin, leftWidth, theme_.px(24),
                             SWP_NOZORDER);

    const int saveWidth = theme_.px(86);
    const int cancelWidth = theme_.px(86);
    const int applyWidth = theme_.px(180);
    // Status text and the action buttons share the bottom row, so there is no
    // empty strip below the buttons.
    const int statusY = height - margin - buttonHeight;
    const int cancelX = width - margin - cancelWidth;
    const int applyX = cancelX - gap - applyWidth;
    const int saveX = applyX - gap - saveWidth;
    saveButton_.SetWindowPos(nullptr, saveX, statusY, saveWidth, buttonHeight,
                             SWP_NOZORDER);
    applyButton_.SetWindowPos(nullptr, applyX, statusY, applyWidth, buttonHeight,
                              SWP_NOZORDER);
    cancelButton_.SetWindowPos(nullptr, cancelX, statusY, cancelWidth, buttonHeight,
                               SWP_NOZORDER);
    status_.SetWindowPos(nullptr, margin, statusY,
                         std::max(0, saveX - margin - gap), buttonHeight,
                         SWP_NOZORDER);

    const int row2Y = statusY - gap - buttonHeight;
    const int row1Y = row2Y - gap - buttonHeight;
    const int third = (leftWidth - 2 * gap) / 3;
    newButton_.SetWindowPos(nullptr, margin, row1Y, third, buttonHeight, SWP_NOZORDER);
    importButton_.SetWindowPos(nullptr, margin + third + gap, row1Y, third, buttonHeight,
                               SWP_NOZORDER);
    copyButton_.SetWindowPos(nullptr, margin + 2 * (third + gap), row1Y,
                             leftWidth - 2 * (third + gap), buttonHeight, SWP_NOZORDER);
    const int half = (leftWidth - gap) / 2;
    deleteButton_.SetWindowPos(nullptr, margin, row2Y, half, buttonHeight, SWP_NOZORDER);
    exportButton_.SetWindowPos(nullptr, margin + half + gap, row2Y,
                               leftWidth - half - gap, buttonHeight, SWP_NOZORDER);

    const int listTop = margin + theme_.px(24) + gap;
    const int listBottom = row1Y - gap;
    themeList_.SetWindowPos(nullptr, margin, listTop, leftWidth,
                            std::max(theme_.px(80), listBottom - listTop), SWP_NOZORDER);

    const int rightX = margin + leftWidth + gap;
    const int rightWidth = width - rightX - margin;
    tabs_.SetWindowPos(nullptr, rightX, margin, rightWidth, theme_.px(26), SWP_NOZORDER);

    const int pageTop = margin + theme_.px(26) + gap;
    const int splitterHeight = theme_.px(8);
    // End everything above the bottom action/status row so the IE control
    // never covers the buttons.
    const int contentBottom = statusY - gap;
    const int span = contentBottom - pageTop - splitterHeight;
    const int minUpper = MinUpperHeight();
    const int minLower = gap + theme_.px(180);
    const int maxUpper = std::max(minUpper, span - minLower);
    const int upperHeight =
        std::clamp(static_cast<int>(span * upperRatio_), minUpper, maxUpper);
    if (span > 0) {
        upperRatio_ = static_cast<double>(upperHeight) / span;
    }
    const int splitterTop = pageTop + upperHeight;
    const int pageBottom = splitterTop - gap;
    const int previewTop = splitterTop + splitterHeight + gap;

    pageRect_ = {rightX, pageTop, rightX + rightWidth, pageBottom};
    splitterRect_ = {rightX, splitterTop, rightX + rightWidth,
                     splitterTop + splitterHeight};
    previewRect_ = {rightX, previewTop, rightX + rightWidth, contentBottom};
    chromeMockRect_ = {};

    splitter_.SetWindowPos(nullptr, splitterRect_.left, splitterRect_.top,
                           splitterRect_.right - splitterRect_.left,
                           splitterRect_.bottom - splitterRect_.top, SWP_NOZORDER);

    if (preview_) {
        preview_->SetBounds(previewRect_);
    }

    const int pageX = pageRect_.left + theme_.px(8);
    const int pageWidth = pageRect_.right - pageRect_.left - theme_.px(16);
    int y = pageRect_.top + theme_.px(8);

    nameEdit_.SetWindowPos(nullptr, pageX, y, pageWidth, theme_.px(24), SWP_NOZORDER);
    y += theme_.px(24) + gap;
    authorEdit_.SetWindowPos(nullptr, pageX, y, pageWidth, theme_.px(24), SWP_NOZORDER);
    y += theme_.px(24) + gap;
    darkCheck_.SetWindowPos(nullptr, pageX, y, theme_.px(140), theme_.px(24),
                            SWP_NOZORDER);
    y += theme_.px(28);
    familyLabel_.SetWindowPos(nullptr, pageX, y + theme_.px(2), theme_.px(52),
                              theme_.px(20), SWP_NOZORDER);
    familyCombo_.SetWindowPos(nullptr, pageX + theme_.px(58), y, theme_.px(210),
                              theme_.px(220), SWP_NOZORDER);
    y += theme_.px(32);
    sourceLabel_.SetWindowPos(nullptr, pageX, y, pageWidth, theme_.px(54),
                              SWP_NOZORDER);

    const int hintY = pageRect_.bottom - theme_.px(22);
    const int editColorY = hintY - gap - theme_.px(24);
    const int pageListTop = pageRect_.top + theme_.px(8);
    const int page = tabs_.GetCurSel();
    const int colorListHeight =
        std::max(theme_.px(60), editColorY - gap - pageListTop);

    colorList_.SetWindowPos(nullptr, pageX, pageListTop, pageWidth, colorListHeight,
                            SWP_NOZORDER);
    editColorButton_.SetWindowPos(nullptr, pageX, editColorY, theme_.px(140),
                                  theme_.px(24), SWP_NOZORDER);

    int derivedTop = pageListTop;
    if (page == 3) {
        const int mockHeight = theme_.px(112);
        chromeMockRect_ = {pageX, pageListTop, pageX + pageWidth,
                           pageListTop + mockHeight};
        derivedTop = chromeMockRect_.bottom + gap;
    }
    derivedList_.SetWindowPos(nullptr, pageX, derivedTop, pageWidth,
                              std::max(theme_.px(60), hintY - gap - derivedTop),
                              SWP_NOZORDER);

    const int explainH = theme_.px(18);
    const int groupH = theme_.px(18);
    const int rowH = theme_.px(28);
    const int editH = theme_.px(22);
    const int fieldLabelH = theme_.px(16);
    const int fieldLabelW = theme_.px(88);
    const int fontEditW =
        std::max(theme_.px(160), pageWidth - fieldLabelW - theme_.px(6));

    int typeY = pageRect_.top + theme_.px(8);
    explainLabel_.SetWindowPos(nullptr, pageX, typeY, pageWidth, explainH,
                               SWP_NOZORDER);
    typeY += explainH + theme_.px(6);

    bodyGroupLabel_.SetWindowPos(nullptr, pageX, typeY, pageWidth, groupH,
                                 SWP_NOZORDER);
    typeY += groupH + theme_.px(2);

    const auto placeLabeledEdit = [&](int y, int x, int labelW, int editW,
                                      CStatic& label, CEdit& edit) {
        label.SetWindowPos(nullptr, x, y + (editH - fieldLabelH) / 2, labelW,
                           fieldLabelH, SWP_NOZORDER);
        edit.SetWindowPos(nullptr, x + labelW + theme_.px(6), y, editW, editH,
                          SWP_NOZORDER);
    };
    placeLabeledEdit(typeY, pageX, fieldLabelW, fontEditW, bodyFontLabel_,
                     bodyFontEdit_);
    typeY += rowH;

    int numericX = pageX;
    placeLabeledEdit(typeY, numericX, theme_.px(92), theme_.px(64),
                     bodySizeLabel_, bodySizeEdit_);
    numericX += theme_.px(92) + theme_.px(6) + theme_.px(64) + gap;
    placeLabeledEdit(typeY, numericX, theme_.px(60), theme_.px(64),
                     lineHeightLabel_, lineHeightEdit_);
    numericX += theme_.px(60) + theme_.px(6) + theme_.px(64) + gap;
    placeLabeledEdit(typeY, numericX, theme_.px(110), theme_.px(80),
                     maxWidthLabel_, maxWidthEdit_);
    typeY += rowH + gap;

    codeGroupLabel_.SetWindowPos(nullptr, pageX, typeY, pageWidth, groupH,
                                 SWP_NOZORDER);
    typeY += groupH + theme_.px(2);
    placeLabeledEdit(typeY, pageX, fieldLabelW, fontEditW, codeFontLabel_,
                     codeFontEdit_);
    typeY += rowH;
    placeLabeledEdit(typeY, pageX, fieldLabelW, theme_.px(64), codeSizeLabel_,
                     codeSizeEdit_);
    typeY += rowH + gap;

    extraCssLabel_.SetWindowPos(nullptr, pageX, typeY, pageWidth, theme_.px(20),
                                SWP_NOZORDER);
    typeY += theme_.px(20) + theme_.px(4);
    extraCssEdit_.SetWindowPos(
        nullptr, pageX, typeY, pageWidth,
        std::max(theme_.px(60), static_cast<int>(pageRect_.bottom) - typeY -
                                    theme_.px(28)),
        SWP_NOZORDER);

    hintLabel_.SetWindowPos(nullptr, pageX, hintY, pageWidth, theme_.px(20),
                            SWP_NOZORDER);

    const BOOL basic = page == 0 ? TRUE : FALSE;
    const BOOL colors = page == 1 ? TRUE : FALSE;
    const BOOL typography = page == 2 ? TRUE : FALSE;
    const BOOL derivedPage = page == 3 ? TRUE : FALSE;
    for (CWindow* control : {static_cast<CWindow*>(&nameEdit_),
                             static_cast<CWindow*>(&authorEdit_),
                             static_cast<CWindow*>(&darkCheck_),
                             static_cast<CWindow*>(&familyLabel_),
                             static_cast<CWindow*>(&familyCombo_),
                             static_cast<CWindow*>(&sourceLabel_)}) {
        control->ShowWindow(basic);
    }
    colorList_.ShowWindow(colors);
    editColorButton_.ShowWindow(colors);
    for (CWindow* control :
         {static_cast<CWindow*>(&explainLabel_),
          static_cast<CWindow*>(&bodyGroupLabel_),
          static_cast<CWindow*>(&codeGroupLabel_),
          static_cast<CWindow*>(&bodyFontLabel_),
          static_cast<CWindow*>(&bodySizeLabel_),
          static_cast<CWindow*>(&lineHeightLabel_),
          static_cast<CWindow*>(&maxWidthLabel_),
          static_cast<CWindow*>(&codeFontLabel_),
          static_cast<CWindow*>(&codeSizeLabel_),
          static_cast<CWindow*>(&bodyFontEdit_),
          static_cast<CWindow*>(&codeFontEdit_),
          static_cast<CWindow*>(&bodySizeEdit_),
          static_cast<CWindow*>(&lineHeightEdit_),
          static_cast<CWindow*>(&maxWidthEdit_),
          static_cast<CWindow*>(&codeSizeEdit_),
          static_cast<CWindow*>(&extraCssLabel_),
          static_cast<CWindow*>(&extraCssEdit_)}) {
        control->ShowWindow(typography);
    }
    derivedList_.ShowWindow(derivedPage);

    UpdateHint();
    Invalidate(FALSE);
}

void ThemeDialog::RunModal()
{
    MSG message = {};
    while (!done_) {
        const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) {
                ::PostQuitMessage(static_cast<int>(message.wParam));
            }
            break;
        }

        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            if (dirty_ && !isBuiltin_) {
                if (::MessageBoxW(m_hWnd, L"当前修改尚未保存，是否放弃？", L"主题设置",
                                  MB_YESNO | MB_ICONQUESTION) != IDYES) {
                    continue;
                }
            }
            CloseDialog();
            continue;
        }

        if (preview_ && preview_->hwnd() != nullptr &&
            (message.hwnd == preview_->hwnd() ||
             ::IsChild(preview_->hwnd(), message.hwnd) != FALSE)) {
            if (preview_->TranslateAccelerator(&message)) {
                continue;
            }
        }

        if (::IsDialogMessageW(m_hWnd, &message) == FALSE) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }
}

void ThemeDialog::CloseDialog()
{
    done_ = true;
    if (IsWindow()) {
        // Enable the owner before the dialog disappears. Otherwise the owner
        // is briefly visible in its disabled state when the modal window goes
        // away, which reads as a flash.
        if (HWND owner = ::GetWindow(m_hWnd, GW_OWNER); owner != nullptr) {
            ::EnableWindow(owner, TRUE);
        }
        // Hide first; ShowModal() destroys the hidden window after the owner
        // has been re-enabled and activated. The final IE/MSHTML teardown then
        // cannot paint anything visible.
        ShowWindow(SW_HIDE);
    }
}

LRESULT ThemeDialog::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    Layout();
    return 0;
}

LRESULT ThemeDialog::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
    if (info != nullptr) {
        info->ptMinTrackSize.x = theme_.px(900);
        info->ptMinTrackSize.y = theme_.px(620);
    }
    return 0;
}

LRESULT ThemeDialog::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(&ps);

    RECT client = {};
    GetClientRect(&client);
    ::FillRect(dc, &client, backgroundBrush_);

    if (chromeMockRect_.right > chromeMockRect_.left) {
        PaintChromeMock(dc, chromeMockRect_);
    }

    EndPaint(&ps);
    return 0;
}

void ThemeDialog::PaintChromeMock(HDC dc, const RECT& area)
{
    if (area.right <= area.left || area.bottom <= area.top) {
        return;
    }
    const theme::ChromeColors& chrome = previewDefinition_.chrome;
    const theme::PreviewColors& preview = previewDefinition_.preview;

    const auto fill = [dc](const RECT& rc, const theme::ThemeColor& color) {
        HBRUSH brush = ::CreateSolidBrush(RGB(color.r, color.g, color.b));
        if (brush != nullptr) {
            ::FillRect(dc, &rc, brush);
            ::DeleteObject(brush);
        }
    };
    const auto frame = [dc](const RECT& rc, const theme::ThemeColor& color) {
        HBRUSH brush = ::CreateSolidBrush(RGB(color.r, color.g, color.b));
        if (brush != nullptr) {
            ::FrameRect(dc, &rc, brush);
            ::DeleteObject(brush);
        }
    };

    FontSelection fonts(dc, theme_.smallFont());
    const int labelH = theme_.px(16);
    RECT label{area.left, area.top, area.right, area.top + labelH};
    ::SetBkMode(dc, OPAQUE);
    ::SetBkColor(dc, ui::ToColorRef(palette_.windowBg));
    ::SetTextColor(dc, ui::ToColorRef(palette_.textSecondary));
    ::DrawTextW(
        dc,
        L"窗口外观示意（标题栏、标签、侧栏、滚动条颜色由 Markdown 配色派生；只读）",
        -1, &label, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    fonts.use(theme_.uiFont());

    RECT body{area.left, area.top + labelH, area.right, area.bottom};
    if (body.bottom <= body.top) {
        return;
    }
    fill(body, chrome.windowBg);

    const int titleH = theme_.px(20);
    const int tabsH = theme_.px(18);
    RECT title{body.left, body.top, body.right, body.top + titleH};
    fill(title, chrome.titleBarBg);
    RECT titleText{title.left + theme_.px(8), title.top,
                   title.right - theme_.px(86), title.bottom};
    ::SetTextColor(dc, ToColorRef(chrome.textPrimary));
    ::SetBkColor(dc, ToColorRef(chrome.titleBarBg));
    ::DrawTextW(dc, L"ActiveXWebMark — 主题预览", -1, &titleText,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    RECT buttons{title.right - theme_.px(80), title.top, title.right - theme_.px(8),
                 title.bottom};
    ::SetTextColor(dc, ToColorRef(chrome.textSecondary));
    ::SetBkColor(dc, ToColorRef(chrome.titleBarBg));
    ::DrawTextW(dc, L"—   □   ×", -1, &buttons,
                DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    RECT tabRow{body.left, title.bottom, body.right, title.bottom + tabsH};
    fill(tabRow, chrome.surface);
    const int tabW =
        std::max(theme_.px(70), static_cast<int>(body.right - body.left) / 4);
    RECT activeTab{tabRow.left + theme_.px(8), tabRow.top,
                   tabRow.left + theme_.px(8) + tabW, tabRow.bottom};
    fill(activeTab, chrome.surface);
    RECT activeLine{activeTab.left, activeTab.bottom - theme_.px(2), activeTab.right,
                    activeTab.bottom};
    RECT inactiveTab{activeTab.right + theme_.px(2), tabRow.top,
                     activeTab.right + theme_.px(2) + tabW, tabRow.bottom};
    fill(inactiveTab, chrome.tabHover);
    ::SetTextColor(dc, ToColorRef(chrome.textPrimary));
    ::SetBkColor(dc, ToColorRef(chrome.surface));
    RECT tabText1{activeTab.left + theme_.px(10), activeTab.top, activeTab.right,
                  activeTab.bottom};
    ::DrawTextW(dc, L"当前文档.md", -1, &tabText1,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    ::SetTextColor(dc, ToColorRef(chrome.textSecondary));
    ::SetBkColor(dc, ToColorRef(chrome.tabHover));
    RECT tabText2{inactiveTab.left + theme_.px(10), inactiveTab.top, inactiveTab.right,
                  inactiveTab.bottom};
    ::DrawTextW(dc, L"其他文档.md", -1, &tabText2,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    // Draw the active underline after the opaque text backgrounds so it is
    // not covered by the tab text background fill.
    fill(activeLine, chrome.accent);

    RECT content{body.left, tabRow.bottom, body.right, body.bottom};
    if (content.bottom <= content.top) {
        return;
    }
    const int sidebarW =
        std::min(theme_.px(150),
                 std::max(theme_.px(60),
                          static_cast<int>(content.right - content.left) / 4));
    RECT sidebar{content.left, content.top, content.left + sidebarW, content.bottom};
    fill(sidebar, chrome.navBg);
    RECT selectedNav{sidebar.left + theme_.px(6), sidebar.top + theme_.px(5),
                     sidebar.right - theme_.px(6),
                     sidebar.top + theme_.px(5) + theme_.px(18)};
    fill(selectedNav, chrome.accentSoft);
    ::SetTextColor(dc, ToColorRef(chrome.textPrimary));
    ::SetBkColor(dc, ToColorRef(chrome.accentSoft));
    RECT selectedText{selectedNav.left + theme_.px(6), selectedNav.top,
                      selectedNav.right, selectedNav.bottom};
    ::DrawTextW(dc, L"目录", -1, &selectedText,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    ::SetTextColor(dc, ToColorRef(chrome.textSecondary));
    ::SetBkColor(dc, ToColorRef(chrome.navBg));
    RECT navText{sidebar.left + theme_.px(12), selectedNav.bottom + theme_.px(4),
                 sidebar.right - theme_.px(6), selectedNav.bottom + theme_.px(20)};
    ::DrawTextW(dc, L"二级标题", -1, &navText,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    RECT document{sidebar.right, content.top, content.right, content.bottom};
    fill(document, chrome.surface);
    const int scrollW = theme_.px(8);
    RECT scrollbar{document.right - scrollW, document.top, document.right,
                   document.bottom};
    fill(scrollbar, preview.pageBg);
    RECT thumb{scrollbar.left + 1, scrollbar.top + theme_.px(4), scrollbar.right - 1,
               scrollbar.top + theme_.px(22)};
    fill(thumb, theme::Blend(preview.pageBg, preview.pageText, 0.28));

    const int textLeft = document.left + theme_.px(10);
    const int textRight = scrollbar.left - theme_.px(6);
    RECT line1{textLeft, document.top + theme_.px(4), textRight,
               document.top + theme_.px(18)};
    ::SetTextColor(dc, ToColorRef(chrome.textPrimary));
    ::SetBkColor(dc, ToColorRef(chrome.surface));
    ::DrawTextW(dc, L"正文示例 — 主题预览", -1, &line1,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    RECT line2{textLeft, line1.bottom + theme_.px(2), textRight,
               line1.bottom + theme_.px(16)};
    ::SetTextColor(dc, ToColorRef(chrome.textSecondary));
    ::SetBkColor(dc, ToColorRef(chrome.surface));
    ::DrawTextW(dc, L"次级文字与选中色", -1, &line2,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    RECT line3{textLeft, line2.bottom + theme_.px(2), textRight,
               line2.bottom + theme_.px(16)};
    ::SetTextColor(dc, ToColorRef(chrome.accent));
    ::SetBkColor(dc, ToColorRef(chrome.surface));
    ::DrawTextW(dc, L"强调色 / 链接", -1, &line3,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    frame(body, chrome.divider);
}

LRESULT ThemeDialog::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

LRESULT ThemeDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    if (dirty_ && !isBuiltin_) {
        if (::MessageBoxW(m_hWnd, L"当前修改尚未保存，是否放弃？", L"主题设置",
                          MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return 0;
        }
    }
    CloseDialog();
    return 0;
}

LRESULT ThemeDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    ::KillTimer(m_hWnd, kPreviewTimerId);
    if (preview_) {
        preview_->Destroy();
        preview_.reset();
    }
    if (backgroundBrush_ != nullptr) {
        ::DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
    if (surfaceBrush_ != nullptr) {
        ::DeleteObject(surfaceBrush_);
        surfaceBrush_ = nullptr;
    }
    done_ = true;
    return 0;
}

LRESULT ThemeDialog::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (wParam == kPreviewTimerId) {
        ::KillTimer(m_hWnd, kPreviewTimerId);
        RenderPreview();
    }
    return 0;
}

LRESULT ThemeDialog::OnCommand(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    const UINT id = LOWORD(wParam);
    const UINT code = HIWORD(wParam);

    switch (id) {
    case IDOK:
        OnOk();
        return 0;
    case IDCANCEL:
        if (dirty_ && !isBuiltin_) {
            if (::MessageBoxW(m_hWnd, L"当前修改尚未保存，是否放弃？", L"主题设置",
                              MB_YESNO | MB_ICONQUESTION) != IDYES) {
                return 0;
            }
        }
        CloseDialog();
        return 0;
    case kNewId:
        CreateNewTheme(false);
        return 0;
    case kCopyId:
        CreateNewTheme(true);
        return 0;
    case kImportId:
        ImportThemes();
        return 0;
    case kDeleteId:
        DeleteCurrentTheme();
        return 0;
    case kExportId:
        ExportCurrentTheme();
        return 0;
    case kEditColorId:
        EditSelectedColor();
        return 0;
    case kApplyId:
        OnApply();
        return 0;
    case kSearchId:
        if (code == EN_CHANGE) {
            RefreshThemeList();
        }
        return 0;
    case kColorListId:
        if (code == LBN_DBLCLK) {
            EditSelectedColor();
        }
        return 0;
    case kNameId:
    case kAuthorId:
    case kBodyFontId:
    case kCodeFontId:
    case kBodySizeId:
    case kLineHeightId:
    case kMaxWidthId:
    case kCodeSizeId:
    case kExtraCssId:
        if (code == EN_CHANGE) {
            OnFieldEdited();
        }
        return 0;
    case kDarkId:
        if (code == BN_CLICKED) {
            OnFieldEdited();
        }
        return 0;
    case kFamilyId:
        if (code == CBN_SELCHANGE) {
            OnFieldEdited();
        }
        return 0;
    default:
        return 0;
    }
}

LRESULT ThemeDialog::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    HDC dc = reinterpret_cast<HDC>(wParam);
    ::SetTextColor(dc, ui::ToColorRef(palette_.textPrimary));
    ::SetBkColor(dc, ui::ToColorRef(palette_.windowBg));
    return reinterpret_cast<LRESULT>(backgroundBrush_);
}

LRESULT ThemeDialog::OnCtlColorEdit(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    HDC dc = reinterpret_cast<HDC>(wParam);
    ::SetTextColor(dc, ui::ToColorRef(palette_.textPrimary));
    ::SetBkColor(dc, ui::ToColorRef(palette_.surface));
    return reinterpret_cast<LRESULT>(surfaceBrush_);
}

LRESULT ThemeDialog::OnCtlColorListBox(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    HDC dc = reinterpret_cast<HDC>(wParam);
    ::SetTextColor(dc, ui::ToColorRef(palette_.textPrimary));
    ::SetBkColor(dc, ui::ToColorRef(palette_.surface));
    return reinterpret_cast<LRESULT>(surfaceBrush_);
}

LRESULT ThemeDialog::OnMeasureItem(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
    if (measure != nullptr &&
        (measure->CtlID == kColorListId || measure->CtlID == kDerivedListId)) {
        measure->itemHeight = static_cast<UINT>(theme_.px(28));
        return TRUE;
    }
    return FALSE;
}

LRESULT ThemeDialog::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
    const bool derived = draw != nullptr && draw->CtlID == kDerivedListId;
    if (draw == nullptr || (!derived && draw->CtlID != kColorListId)) {
        return FALSE;
    }
    if (draw->itemID == static_cast<UINT>(-1)) {
        return TRUE;
    }
    const int row = static_cast<int>(draw->itemData);
    if (row < 0) {
        return TRUE;
    }

    const wchar_t* label = nullptr;
    theme::ThemeColor color;
    if (derived) {
        if (static_cast<std::size_t>(row) >= derivedRows_.size()) {
            return TRUE;
        }
        label = derivedRows_[static_cast<std::size_t>(row)].label;
        color = derivedRows_[static_cast<std::size_t>(row)].access(previewDefinition_);
    } else {
        if (static_cast<std::size_t>(row) >= colorRows_.size()) {
            return TRUE;
        }
        label = colorRows_[static_cast<std::size_t>(row)].label;
        color = colorRows_[static_cast<std::size_t>(row)].access(draft_);
    }
    const bool selected = (draw->itemState & ODS_SELECTED) != 0;
    HBRUSH background = ::CreateSolidBrush(ui::ToColorRef(
        selected ? palette_.accentSoft : palette_.surface));
    ::FillRect(draw->hDC, &draw->rcItem, background);
    ::DeleteObject(background);

    RECT swatch{draw->rcItem.left + theme_.px(4),
                draw->rcItem.top + theme_.px(5),
                draw->rcItem.left + theme_.px(24),
                draw->rcItem.bottom - theme_.px(5)};
    HBRUSH swatchBrush = ::CreateSolidBrush(RGB(color.r, color.g, color.b));
    ::FillRect(draw->hDC, &swatch, swatchBrush);
    ::DeleteObject(swatchBrush);
    HBRUSH frame = ::CreateSolidBrush(ui::ToColorRef(palette_.divider));
    ::FrameRect(draw->hDC, &swatch, frame);
    ::DeleteObject(frame);

    ::SetBkMode(draw->hDC, TRANSPARENT);
    ::SetTextColor(draw->hDC, ui::ToColorRef(palette_.textPrimary));
    RECT labelRect{swatch.right + theme_.px(8), draw->rcItem.top,
                   draw->rcItem.right - theme_.px(80), draw->rcItem.bottom};
    ::DrawTextW(draw->hDC, label, -1, &labelRect,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    ::SetTextColor(draw->hDC, ui::ToColorRef(palette_.textSecondary));
    const std::wstring value = utf8ToWide(theme::ColorToCss(color));
    RECT valueRect{draw->rcItem.left, draw->rcItem.top,
                   draw->rcItem.right - theme_.px(6), draw->rcItem.bottom};
    ::DrawTextW(draw->hDC, value.c_str(), -1, &valueRect,
                DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    if ((draw->itemState & ODS_FOCUS) != 0) {
        RECT focus = draw->rcItem;
        ::InflateRect(&focus, -1, -1);
        ::DrawFocusRect(draw->hDC, &focus);
    }
    return TRUE;
}

LRESULT ThemeDialog::OnThemeSelectionChanged(int, LPNMHDR header, BOOL&)
{
    auto* info = reinterpret_cast<NMLISTVIEW*>(header);
    if (info == nullptr || info->iItem < 0) {
        return 0;
    }
    if ((info->uChanged & LVIF_STATE) == 0 ||
        (info->uNewState & LVIS_SELECTED) == 0) {
        return 0;
    }
    if (suppressSelection_) {
        return 0;
    }
    const int catalogIndex =
        static_cast<int>(themeList_.GetItemData(info->iItem));
    if (catalogIndex == currentIndex_) {
        return 0;
    }

    if (dirty_ && !isBuiltin_) {
        if (::MessageBoxW(m_hWnd, L"当前主题的修改尚未保存，是否放弃？", L"主题设置",
                          MB_YESNO | MB_ICONQUESTION) != IDYES) {
            suppressSelection_ = true;
            if (currentIndex_ < 0) {
                themeList_.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            } else {
                for (int i = 0; i < themeList_.GetItemCount(); ++i) {
                    if (static_cast<int>(themeList_.GetItemData(i)) == currentIndex_) {
                        themeList_.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED,
                                                LVIS_SELECTED | LVIS_FOCUSED);
                        break;
                    }
                }
            }
            suppressSelection_ = false;
            return 0;
        }
    }
    LoadThemeIntoEditor(catalogIndex);
    return 0;
}

LRESULT ThemeDialog::OnTabSelChange(int, LPNMHDR, BOOL&)
{
    Layout();
    return 0;
}

} // namespace awm
