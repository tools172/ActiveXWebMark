#pragma once

#include <windows.h>
#include <ole2.h>

#include <functional>
#include <string>
#include <vector>

struct IDropTargetHelper;

namespace awm::ui {

/// OLE drop target that accepts local files (CF_HDROP) and reports their paths.
///
/// The target is registered with RegisterDragDrop() on a window. OLE resolves
/// the window under the cursor and walks up the parent chain until it finds a
/// registered target, so registering on the main window covers every child
/// control that does not register a target itself (tab strip, splitters, the
/// navigation tree, the title bar, ...). Windows that do have their own target
/// (Scintilla, MSHTML) must be taken over explicitly with Register(..., true)
/// or by revoking first; an existing target can be passed as `forward` so
/// non-file drops keep their original behaviour.
///
/// The target also drives IDropTargetHelper, which draws the source-provided
/// drag image (for Explorer that is the file icon ghost) and its drop effect.
class FileDropTarget final : public IDropTarget {
public:
    using FilesHandler = std::function<void(std::vector<std::wstring>)>;

    /// Returns true when a dropped path can be opened. Empty accepts any file.
    using FilesFilter = std::function<bool(const std::wstring&)>;

    /// `forward` (optional, not owned) receives drags that do not carry files.
    explicit FileDropTarget(FilesHandler handler, FilesFilter filter = {},
                            IDropTarget* forward = nullptr);
    ~FileDropTarget();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDropTarget
    STDMETHODIMP DragEnter(IDataObject* dataObject, DWORD keyState, POINTL pt,
                          DWORD* effect) override;
    STDMETHODIMP DragOver(DWORD keyState, POINTL pt, DWORD* effect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop(IDataObject* dataObject, DWORD keyState, POINTL pt,
                      DWORD* effect) override;

    /// Registers `target` on `hwnd`. When `replaceExisting` is true the window's
    /// current drop target (if any) is revoked first. OLE takes its own
    /// reference on success; the caller keeps its original reference.
    static bool Register(HWND hwnd, FileDropTarget* target, bool replaceExisting = false);

    /// Revokes the registration on `hwnd` and releases the caller's reference.
    static void Unregister(HWND hwnd, FileDropTarget*& target);

    /// Returns the drop target currently registered for `hwnd`, or nullptr.
    ///
    /// OLE stores the registered pointer in the window's
    /// "OleDropTargetInterface" property; this lets a caller take over an
    /// existing target (Scintilla) and forward non-file drags to it.
    static IDropTarget* RegisteredTarget(HWND hwnd);

private:
    static std::vector<std::wstring> ExtractFiles(IDataObject* dataObject);
    static bool HasFiles(IDataObject* dataObject);
    static DWORD CopyEffect(DWORD allowed);

    void EnsureDropHelper();
    void HelperEnter(IDataObject* dataObject, POINTL pt, DWORD effect);
    void HelperOver(POINTL pt, DWORD effect);
    void HelperLeave();
    void HelperDrop(IDataObject* dataObject, POINTL pt, DWORD effect);

    LONG refs_ = 1;
    FilesHandler handler_;
    FilesFilter filter_;
    IDropTarget* forward_ = nullptr; ///< Not owned; lives as long as the window.
    HWND hwnd_ = nullptr;            ///< First window the target was registered on.
    IDropTargetHelper* dropHelper_ = nullptr;
    std::vector<std::wstring> pendingFiles_;
    bool fileDrag_ = false;    ///< The current drag carries CF_HDROP files.
    bool acceptsFiles_ = false; ///< At least one file passed the filter.
    bool forwarded_ = false;
    bool helperActive_ = false;
};

} // namespace awm::ui
