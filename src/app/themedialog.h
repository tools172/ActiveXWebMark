#pragma once

#include <Windows.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "controls/splitter.h"
#include "ipreviewview.h"
#include "previewassets.h"
#include "theme.h"
#include "theme/theme_catalog.h"
#include "theme/theme_source.h"
#include "theme/theme_store.h"
#include "wtlinclude.h"

namespace awm {

struct ThemeDialogResult {
    /// True when the user explicitly asked to switch to the selected theme.
    bool applyRequested = false;
    /// True when that theme was already applied while the dialog was open, so
    /// the owner must not apply it a second time when the dialog closes.
    bool appliedWhileOpen = false;
    bool catalogChanged = false;
    /// Theme id to apply when applyRequested is true.
    std::string selectedThemeId;
    /// User theme that was saved (empty for built-ins / cancel).
    std::string savedThemeId;
};

/// Modal theme browser/editor. Built-in themes are read-only until copied;
/// user themes are edited and written back as `*.awmtheme.json`.
class ThemeDialog : public CWindowImpl<ThemeDialog> {
public:
    DECLARE_WND_CLASS_EX(L"AwmThemeDialog", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    ~ThemeDialog() override;

    /// Called when the user clicks "立即使用此主题". Applying while the dialog
    /// stays open lets the owner update the live window behind it.
    using ApplyCallback = std::function<void(const std::string& themeId)>;

    static ThemeDialogResult ShowModal(HWND owner, const std::string& currentThemeId,
                                       const std::filesystem::path& themesDirectory,
                                       const PreviewAssets* previewAssets,
                                       const ui::Palette& palette,
                                       ApplyCallback onApply = {});

    BEGIN_MSG_MAP(ThemeDialog)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
        MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnCtlColorEdit)
        MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, OnCtlColorListBox)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)
        NOTIFY_HANDLER(kThemeListId, LVN_ITEMCHANGED, OnThemeSelectionChanged)
        NOTIFY_HANDLER(kTabsId, TCN_SELCHANGE, OnTabSelChange)
    END_MSG_MAP()

private:
    static constexpr UINT kSearchId = 200;
    static constexpr UINT kThemeListId = 201;
    static constexpr UINT kTabsId = 202;
    static constexpr UINT kNameId = 203;
    static constexpr UINT kAuthorId = 204;
    static constexpr UINT kDarkId = 205;
    static constexpr UINT kFamilyId = 206;
    static constexpr UINT kColorListId = 207;
    static constexpr UINT kEditColorId = 208;
    static constexpr UINT kBodyFontId = 209;
    static constexpr UINT kCodeFontId = 210;
    static constexpr UINT kBodySizeId = 211;
    static constexpr UINT kLineHeightId = 212;
    static constexpr UINT kMaxWidthId = 213;
    static constexpr UINT kCodeSizeId = 214;
    static constexpr UINT kExtraCssId = 215;
    static constexpr UINT kNewId = 216;
    static constexpr UINT kImportId = 217;
    static constexpr UINT kCopyId = 218;
    static constexpr UINT kDeleteId = 219;
    static constexpr UINT kExportId = 220;
    static constexpr UINT kStatusId = 221;
    static constexpr UINT kHintId = 222;
    static constexpr UINT kSourceId = 223;
    static constexpr UINT kFamilyLabelId = 224;
    static constexpr UINT kExtraCssLabelId = 225;
    static constexpr UINT kApplyId = 226;
    static constexpr UINT kDerivedListId = 227;
    static constexpr UINT kSplitterId = 229;
    static constexpr UINT kExplainId = 230;
    static constexpr UINT kBodyGroupId = 231;
    static constexpr UINT kCodeGroupId = 232;
    static constexpr UINT kBodyFontLabelId = 233;
    static constexpr UINT kBodySizeLabelId = 234;
    static constexpr UINT kLineHeightLabelId = 235;
    static constexpr UINT kMaxWidthLabelId = 236;
    static constexpr UINT kCodeFontLabelId = 237;
    static constexpr UINT kCodeSizeLabelId = 238;
    static constexpr UINT kPreviewTimerId = 1;
    static constexpr UINT kPreviewDelayMs = 150;

    struct ColorRow {
        const wchar_t* label;
        /// Name in ThemeSource::explicitFields (see theme::field).
        const char* key;
        std::function<theme::ThemeColor&(theme::ThemeSource&)> access;
    };

    struct DerivedRow {
        const wchar_t* label;
        std::function<theme::ThemeColor(const theme::ThemeDefinition&)> access;
    };

    ThemeDialog(std::string currentThemeId, std::filesystem::path themesDirectory,
                const PreviewAssets* previewAssets, ui::Palette palette,
                ApplyCallback onApply);

    bool CreateModalWindow(HWND owner);
    void CreateChildren();
    void Layout();
    void RunModal();
    void ApplyThemeToDialog();

    void ReloadStore();
    void RefreshThemeList();
    void LoadThemeIntoEditor(int catalogIndex);
    void FillControlsFromDraft();
    void ReadControlsIntoDraft();
    void UpdateEditorEnabledState();
    void UpdateHint();
    /// Hides first, then destroys, so the last paint/IE teardown is not visible.
    void CloseDialog();
    void RefreshPreviewDefinition();
    void PaintChromeMock(HDC dc, const RECT& area);
    void RenderPreview();
    void SchedulePreview();

    int MinUpperHeight() const;
    int ClampSplitTop(int y, int* contentTop = nullptr, int* span = nullptr) const;
    void PreviewSplitDrag(int y);
    void CommitSplitDrag(int y);
    /// Moves only the splitter-dependent controls during a live drag.
    void LayoutSplitter();
    void CreateNewTheme(bool duplicate);
    void ImportThemes();
    void DeleteCurrentTheme();
    void ExportCurrentTheme();
    void EditSelectedColor();
    void SelectThemeId(const std::string& id);
    void SetStatus(const std::wstring& text);

    void OnFieldEdited();
    void OnOk();
    void OnApply();
    bool SaveCurrentTheme(std::string& savedId);
    std::wstring TextOf(CWindow& control) const;
    std::string Utf8TextOf(CWindow& control) const;
    void SetText(CWindow& control, const std::string& utf8);

    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCtlColorEdit(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCtlColorListBox(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDrawItem(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMeasureItem(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnThemeSelectionChanged(int, LPNMHDR, BOOL&);
    LRESULT OnTabSelChange(int, LPNMHDR, BOOL&);

    ui::Theme theme_;
    ui::Palette palette_;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH surfaceBrush_ = nullptr;

    CEdit searchEdit_;
    CListViewCtrl themeList_;
    CButton newButton_;
    CButton importButton_;
    CButton copyButton_;
    CButton deleteButton_;
    CButton exportButton_;
    CButton saveButton_;
    CButton applyButton_;
    CButton cancelButton_;

    CTabCtrl tabs_;
    CEdit nameEdit_;
    CEdit authorEdit_;
    CButton darkCheck_;
    CComboBox familyCombo_;
    CStatic familyLabel_;
    CStatic sourceLabel_;
    CListBox colorList_;
    CButton editColorButton_;
    CListBox derivedList_;
    CEdit bodyFontEdit_;
    CEdit codeFontEdit_;
    CEdit bodySizeEdit_;
    CEdit lineHeightEdit_;
    CEdit maxWidthEdit_;
    CEdit codeSizeEdit_;
    CStatic explainLabel_;
    CStatic bodyGroupLabel_;
    CStatic codeGroupLabel_;
    CStatic bodyFontLabel_;
    CStatic bodySizeLabel_;
    CStatic lineHeightLabel_;
    CStatic maxWidthLabel_;
    CStatic codeFontLabel_;
    CStatic codeSizeLabel_;
    CStatic extraCssLabel_;
    CEdit extraCssEdit_;
    CStatic hintLabel_;
    CStatic status_;
    ui::Splitter splitter_;
    std::unique_ptr<IPreviewView> preview_;

    std::string currentThemeId_;
    std::filesystem::path themesDirectory_;
    const PreviewAssets* previewAssets_ = nullptr;
    ApplyCallback onApply_;
    std::vector<theme::StoredTheme> userThemes_;
    std::vector<ColorRow> colorRows_;
    std::vector<DerivedRow> derivedRows_;

    theme::ThemeSource draft_;
    theme::ThemeDefinition previewDefinition_;
    /// Colors the user changed in this dialog session. They stay fixed when a
    /// base color later re-derives the structural colors around it.
    std::set<std::string> sessionEditedFields_;

    int currentIndex_ = -1;
    int themeSelectionWidth_ = 0;
    bool isBuiltin_ = true;
    bool dirty_ = false;
    bool applyRequested_ = false;
    bool catalogChanged_ = false;
    bool done_ = false;
    bool suppressSelection_ = false;
    bool suppressFieldChanges_ = false;
    std::string selectedThemeId_;
    std::string savedThemeId_;
    /// Theme applied through "立即使用此主题" while the dialog stayed open.
    std::string appliedThemeId_;
    COLORREF customColors_[16] = {};

    RECT pageRect_ = {};
    RECT splitterRect_ = {};
    RECT chromeMockRect_ = {};
    RECT previewRect_ = {};
    double upperRatio_ = 0.58;
};

} // namespace awm
