#include "controls/filedroptarget.h"

#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <utility>

namespace awm::ui {

namespace {

constexpr wchar_t kDropTargetProperty[] = L"OleDropTargetInterface";

// CLSID_DragDropHelper. Declared locally so the shell GUID does not require
// linking uuid.lib just for this class.
constexpr CLSID kDragDropHelper = {
    0x4657278a,
    0x411b,
    0x11d2,
    {0x83, 0x9a, 0x00, 0xc0, 0x4f, 0xd9, 0x18, 0xd0}};

FORMATETC fileFormat()
{
    FORMATETC format = {};
    format.cfFormat = CF_HDROP;
    format.dwAspect = DVASPECT_CONTENT;
    format.lindex = -1;
    format.tymed = TYMED_HGLOBAL;
    return format;
}

} // namespace

FileDropTarget::FileDropTarget(FilesHandler handler, FilesFilter filter,
                               IDropTarget* forward)
    : handler_(std::move(handler))
    , filter_(std::move(filter))
    , forward_(forward)
{
}

FileDropTarget::~FileDropTarget()
{
    if (dropHelper_ != nullptr) {
        dropHelper_->Release();
    }
}

STDMETHODIMP FileDropTarget::QueryInterface(REFIID riid, void** ppvObject)
{
    if (ppvObject == nullptr) {
        return E_POINTER;
    }
    *ppvObject = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropTarget)) {
        *ppvObject = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) FileDropTarget::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&refs_));
}

STDMETHODIMP_(ULONG) FileDropTarget::Release()
{
    const LONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) {
        delete this;
    }
    return static_cast<ULONG>(refs);
}

bool FileDropTarget::HasFiles(IDataObject* dataObject)
{
    if (dataObject == nullptr) {
        return false;
    }
    FORMATETC format = fileFormat();
    return dataObject->QueryGetData(&format) == S_OK;
}

DWORD FileDropTarget::CopyEffect(DWORD allowed)
{
    return (allowed & DROPEFFECT_COPY) != 0 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

std::vector<std::wstring> FileDropTarget::ExtractFiles(IDataObject* dataObject)
{
    std::vector<std::wstring> files;
    if (dataObject == nullptr) {
        return files;
    }

    FORMATETC format = fileFormat();
    STGMEDIUM medium = {};
    if (dataObject->GetData(&format, &medium) != S_OK) {
        return files;
    }

    if (medium.tymed == TYMED_HGLOBAL && medium.hGlobal != nullptr) {
        const HDROP drop = static_cast<HDROP>(medium.hGlobal);
        const UINT count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        files.reserve(count);
        for (UINT i = 0; i < count; ++i) {
            const UINT length = ::DragQueryFileW(drop, i, nullptr, 0);
            if (length == 0) {
                continue;
            }
            std::wstring path(static_cast<std::size_t>(length) + 1, L'\0');
            if (::DragQueryFileW(drop, i, path.data(), length + 1) == length) {
                path.resize(length);
                files.push_back(std::move(path));
            }
        }
    }

    ::ReleaseStgMedium(&medium);
    return files;
}

void FileDropTarget::EnsureDropHelper()
{
    if (dropHelper_ != nullptr) {
        return;
    }
    IDropTargetHelper* helper = nullptr;
    if (SUCCEEDED(::CoCreateInstance(kDragDropHelper, nullptr, CLSCTX_INPROC_SERVER,
                                     __uuidof(IDropTargetHelper),
                                     reinterpret_cast<void**>(&helper)))) {
        dropHelper_ = helper;
    }
}

void FileDropTarget::HelperEnter(IDataObject* dataObject, POINTL pt, DWORD effect)
{
    EnsureDropHelper();
    if (dropHelper_ == nullptr) {
        return;
    }
    POINT point = {pt.x, pt.y};
    // Use the window OLE targeted (deepest ancestor of the cursor with a
    // registered drop target) owned by the top-level frame. A shell drag image
    // owned by a child of the embedded browser is not displayed.
    HWND target = ::WindowFromPoint(point);
    while (target != nullptr && ::GetPropW(target, kDropTargetProperty) == nullptr) {
        target = ::GetParent(target);
    }
    if (target == nullptr) {
        target = hwnd_;
    }
    if (target == nullptr) {
        return;
    }
    if (HWND root = ::GetAncestor(target, GA_ROOT)) {
        target = root;
    }
    if (SUCCEEDED(dropHelper_->DragEnter(target, dataObject, &point, effect))) {
        helperActive_ = true;
    }
}

void FileDropTarget::HelperOver(POINTL pt, DWORD effect)
{
    if (dropHelper_ == nullptr || !helperActive_) {
        return;
    }
    POINT point = {pt.x, pt.y};
    dropHelper_->DragOver(&point, effect);
}

void FileDropTarget::HelperLeave()
{
    if (dropHelper_ != nullptr && helperActive_) {
        helperActive_ = false;
        dropHelper_->DragLeave();
    }
}

void FileDropTarget::HelperDrop(IDataObject* dataObject, POINTL pt, DWORD effect)
{
    if (dropHelper_ == nullptr || !helperActive_) {
        return;
    }
    helperActive_ = false;
    POINT point = {pt.x, pt.y};
    dropHelper_->Drop(dataObject, &point, effect);
}

STDMETHODIMP FileDropTarget::DragEnter(IDataObject* dataObject, DWORD keyState, POINTL pt,
                                       DWORD* effect)
{
    if (effect == nullptr) {
        return E_POINTER;
    }

    fileDrag_ = false;
    acceptsFiles_ = false;
    forwarded_ = false;
    pendingFiles_.clear();

    if (HasFiles(dataObject)) {
        fileDrag_ = true;
        pendingFiles_ = ExtractFiles(dataObject);
        if (filter_) {
            pendingFiles_.erase(
                std::remove_if(pendingFiles_.begin(), pendingFiles_.end(),
                               [this](const std::wstring& path) { return !filter_(path); }),
                pendingFiles_.end());
        }
        acceptsFiles_ = !pendingFiles_.empty();
        // Unsupported files keep the drag image but show the no-drop cursor,
        // so the user gets immediate feedback instead of an error afterwards.
        *effect = acceptsFiles_ ? CopyEffect(*effect) : DROPEFFECT_NONE;
        HelperEnter(dataObject, pt, *effect);
        return S_OK;
    }

    if (forward_ != nullptr) {
        forwarded_ = true;
        const HRESULT hr = forward_->DragEnter(dataObject, keyState, pt, effect);
        if (FAILED(hr)) {
            forwarded_ = false;
            return hr;
        }
        return S_OK;
    }

    *effect = DROPEFFECT_NONE;
    return S_OK;
}

STDMETHODIMP FileDropTarget::DragOver(DWORD keyState, POINTL pt, DWORD* effect)
{
    if (effect == nullptr) {
        return E_POINTER;
    }
    if (fileDrag_) {
        *effect = acceptsFiles_ ? CopyEffect(*effect) : DROPEFFECT_NONE;
        HelperOver(pt, *effect);
        return S_OK;
    }
    if (forwarded_ && forward_ != nullptr) {
        return forward_->DragOver(keyState, pt, effect);
    }
    *effect = DROPEFFECT_NONE;
    return S_OK;
}

STDMETHODIMP FileDropTarget::DragLeave()
{
    fileDrag_ = false;
    acceptsFiles_ = false;
    pendingFiles_.clear();
    HelperLeave();

    if (forwarded_ && forward_ != nullptr) {
        forwarded_ = false;
        return forward_->DragLeave();
    }
    forwarded_ = false;
    return S_OK;
}

STDMETHODIMP FileDropTarget::Drop(IDataObject* dataObject, DWORD keyState, POINTL pt,
                                  DWORD* effect)
{
    if (effect == nullptr) {
        return E_POINTER;
    }

    if (fileDrag_) {
        *effect = acceptsFiles_ ? CopyEffect(*effect) : DROPEFFECT_NONE;
        HelperDrop(dataObject, pt, *effect);

        if (acceptsFiles_ && handler_ && !pendingFiles_.empty()) {
            handler_(std::move(pendingFiles_));
        }
        pendingFiles_.clear();
        fileDrag_ = false;
        acceptsFiles_ = false;
        return S_OK;
    }

    if (forwarded_ && forward_ != nullptr) {
        forwarded_ = false;
        return forward_->Drop(dataObject, keyState, pt, effect);
    }

    forwarded_ = false;
    *effect = DROPEFFECT_NONE;
    return S_OK;
}

bool FileDropTarget::Register(HWND hwnd, FileDropTarget* target, bool replaceExisting)
{
    if (hwnd == nullptr || !::IsWindow(hwnd) || target == nullptr) {
        return false;
    }
    if (replaceExisting) {
        ::RevokeDragDrop(hwnd);
    }
    if (FAILED(::RegisterDragDrop(hwnd, target))) {
        return false;
    }
    if (target->hwnd_ == nullptr) {
        target->hwnd_ = hwnd;
    }
    return true;
}

void FileDropTarget::Unregister(HWND hwnd, FileDropTarget*& target)
{
    if (hwnd != nullptr && ::IsWindow(hwnd)) {
        ::RevokeDragDrop(hwnd);
    }
    if (target != nullptr) {
        target->Release();
        target = nullptr;
    }
}

IDropTarget* FileDropTarget::RegisteredTarget(HWND hwnd)
{
    if (hwnd == nullptr) {
        return nullptr;
    }
    return static_cast<IDropTarget*>(::GetPropW(hwnd, kDropTargetProperty));
}

} // namespace awm::ui
