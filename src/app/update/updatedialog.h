#pragma once

#include <functional>
#include <string>

#include "theme.h"
#include "wtlinclude.h"

namespace awm {

/// Small floating window that reports update checks, download progress and
/// the ready-to-install state. It is modeless and owned by the main window, so
/// the user can keep using the application while a download runs.
class UpdateDialog : public CWindowImpl<UpdateDialog> {
public:
    DECLARE_WND_CLASS_EX(L"AwmUpdateDialog", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    enum class Mode {
        Checking,
        Downloading,
        Ready,
        Error,
    };

    struct Callbacks {
        /// Cancel button, "稍后", or the window close button.
        std::function<void()> onCancel;
        /// "立即安装" in the ready state.
        std::function<void()> onInstall;
        /// "重试下载" after a download error.
        std::function<void()> onRetry;
    };

    ~UpdateDialog() override;

    bool Create(HWND owner);
    void SetCallbacks(Callbacks callbacks);

    void ShowChecking(const std::wstring& currentVersion);
    void ShowDownloading(const std::wstring& version, const std::wstring& assetName,
                         long long expectedBytes);
    void SetProgress(long long downloaded, long long total);
    void ShowReady(const std::wstring& version);
    void ShowError(const std::wstring& message);

    /// Destroys the window without invoking any callback.
    void CloseSilently();

    Mode mode() const { return mode_; }
    bool IsOpen() const { return IsWindow() != FALSE; }

    static constexpr UINT kCancelId = 101;
    static constexpr UINT kInstallId = 102;
    static constexpr UINT kRetryId = 103;

    BEGIN_MSG_MAP(UpdateDialog)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChangedMessage)
        COMMAND_ID_HANDLER(kCancelId, OnCancelButton)
        COMMAND_ID_HANDLER(kInstallId, OnInstallButton)
        COMMAND_ID_HANDLER(kRetryId, OnRetryButton)
        COMMAND_ID_HANDLER(IDOK, OnInstallButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

private:
    static constexpr UINT kTitleId = 10;
    static constexpr UINT kStatusId = 11;
    static constexpr UINT kProgressId = 12;
    static constexpr UINT kDetailId = 13;

    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDpiChangedMessage(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCancelButton(WORD, WORD, HWND, BOOL&);
    LRESULT OnInstallButton(WORD, WORD, HWND, BOOL&);
    LRESULT OnRetryButton(WORD, WORD, HWND, BOOL&);

    void createChildren();
    void applyFonts();
    void layoutChildren();
    void showMode();
    void closeWith(const std::function<void()>& callback);
    void setButton(CButton& button, bool visible, const wchar_t* text);
    void setIndeterminate(bool indeterminate);
    void setDeterminateProgress(long long downloaded, long long total);
    void setErrorState();

    ui::Theme theme_;
    CStatic title_;
    CStatic status_;
    CStatic detail_;
    CProgressBarCtrl progress_;
    CButton cancelButton_;
    CButton installButton_;
    CButton retryButton_;
    HBRUSH backgroundBrush_ = nullptr;
    Callbacks callbacks_;
    Mode mode_ = Mode::Checking;
    bool closing_ = false;
};

} // namespace awm
