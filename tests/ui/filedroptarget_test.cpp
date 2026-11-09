#include "controls/filedroptarget.h"

#include <shellapi.h>
#include <shlobj.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace awm::ui {
namespace {

// Minimal in-memory data object exposing a CF_HDROP file list.
class FakeDataObject final : public IDataObject {
public:
    explicit FakeDataObject(std::vector<std::wstring> files)
        : files_(std::move(files))
    {
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (ppv == nullptr) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDataObject)) {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refs_));
    }
    STDMETHODIMP_(ULONG) Release() override
    {
        const LONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) {
            delete this;
        }
        return static_cast<ULONG>(refs);
    }

    STDMETHODIMP GetData(FORMATETC* format, STGMEDIUM* medium) override
    {
        if (format == nullptr || medium == nullptr) {
            return E_POINTER;
        }
        if (format->cfFormat != CF_HDROP || files_.empty()) {
            return DV_E_FORMATETC;
        }

        std::size_t chars = 1;
        for (const std::wstring& file : files_) {
            chars += file.size() + 1;
        }
        const std::size_t bytes = sizeof(DROPFILES) + chars * sizeof(wchar_t);
        HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (memory == nullptr) {
            return E_OUTOFMEMORY;
        }
        auto* drop = static_cast<DROPFILES*>(::GlobalLock(memory));
        if (drop == nullptr) {
            ::GlobalFree(memory);
            return E_OUTOFMEMORY;
        }
        drop->pFiles = sizeof(DROPFILES);
        drop->fWide = TRUE;
        auto* cursor = reinterpret_cast<wchar_t*>(
            reinterpret_cast<BYTE*>(drop) + sizeof(DROPFILES));
        for (const std::wstring& file : files_) {
            ::memcpy(cursor, file.c_str(), (file.size() + 1) * sizeof(wchar_t));
            cursor += file.size() + 1;
        }
        ::GlobalUnlock(memory);

        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = memory;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }
    STDMETHODIMP GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    STDMETHODIMP QueryGetData(FORMATETC* format) override
    {
        if (format == nullptr) {
            return E_POINTER;
        }
        return format->cfFormat == CF_HDROP && !files_.empty() ? S_OK : DV_E_FORMATETC;
    }
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override { return E_NOTIMPL; }
    STDMETHODIMP SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    STDMETHODIMP EnumFormatEtc(DWORD, IEnumFORMATETC**) override { return E_NOTIMPL; }
    STDMETHODIMP DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return E_NOTIMPL; }
    STDMETHODIMP DUnadvise(DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }

private:
    LONG refs_ = 1;
    std::vector<std::wstring> files_;
};

// Records forwarded drag calls so the non-file path can be checked.
class RecordingTarget final : public IDropTarget {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (ppv == nullptr) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropTarget)) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refs_));
    }
    STDMETHODIMP_(ULONG) Release() override
    {
        const LONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) {
            delete this;
        }
        return static_cast<ULONG>(refs);
    }

    STDMETHODIMP DragEnter(IDataObject*, DWORD, POINTL, DWORD* effect) override
    {
        entered = true;
        *effect = DROPEFFECT_MOVE;
        return S_OK;
    }
    STDMETHODIMP DragOver(DWORD, POINTL, DWORD* effect) override
    {
        over = true;
        *effect = DROPEFFECT_MOVE;
        return S_OK;
    }
    STDMETHODIMP DragLeave() override
    {
        left = true;
        return S_OK;
    }
    STDMETHODIMP Drop(IDataObject*, DWORD, POINTL, DWORD* effect) override
    {
        dropped = true;
        *effect = DROPEFFECT_MOVE;
        return S_OK;
    }

    bool entered = false;
    bool over = false;
    bool left = false;
    bool dropped = false;

private:
    LONG refs_ = 1;
};

} // namespace

TEST(FileDropTarget, DragEnterAcceptsFilesWithCopy)
{
    std::vector<std::wstring> received;
    FileDropTarget target([&](std::vector<std::wstring> files) {
        received = std::move(files);
    });

    FakeDataObject data({L"C:\\notes\\one.md", L"C:\\notes\\two.md"});
    DWORD effect = DROPEFFECT_COPY;
    POINTL point = {0, 0};
    EXPECT_EQ(target.DragEnter(&data, 0, point, &effect), S_OK);
    EXPECT_EQ(effect, DROPEFFECT_COPY);
    EXPECT_TRUE(received.empty()); // Nothing is delivered before Drop.

    effect = DROPEFFECT_COPY;
    EXPECT_EQ(target.Drop(&data, 0, point, &effect), S_OK);
    EXPECT_EQ(effect, DROPEFFECT_COPY);
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0], L"C:\\notes\\one.md");
    EXPECT_EQ(received[1], L"C:\\notes\\two.md");
}

TEST(FileDropTarget, DragEnterRejectsDragsWithoutFiles)
{
    FileDropTarget target([](std::vector<std::wstring>) {});
    FakeDataObject data({});

    DWORD effect = DROPEFFECT_COPY;
    POINTL point = {0, 0};
    EXPECT_EQ(target.DragEnter(&data, 0, point, &effect), S_OK);
    EXPECT_EQ(effect, DROPEFFECT_NONE);
}

TEST(FileDropTarget, ForwardsNonFileDragsToPreviousTarget)
{
    auto* recording = new RecordingTarget();
    std::vector<std::wstring> received;
    FileDropTarget target(
        [&](std::vector<std::wstring> files) { received = std::move(files); }, {}, recording);

    FakeDataObject data({});
    DWORD effect = DROPEFFECT_COPY;
    POINTL point = {0, 0};

    EXPECT_EQ(target.DragEnter(&data, 0, point, &effect), S_OK);
    EXPECT_TRUE(recording->entered);
    EXPECT_EQ(effect, DROPEFFECT_MOVE);

    EXPECT_EQ(target.Drop(&data, 0, point, &effect), S_OK);
    EXPECT_TRUE(recording->dropped);
    EXPECT_TRUE(received.empty());

    recording->Release();
}

TEST(FileDropTarget, FileDragsAreNotForwarded)
{
    auto* recording = new RecordingTarget();
    FileDropTarget target([](std::vector<std::wstring>) {}, {}, recording);

    FakeDataObject data({L"C:\\notes\\one.md"});
    DWORD effect = DROPEFFECT_COPY;
    POINTL point = {0, 0};

    EXPECT_EQ(target.DragEnter(&data, 0, point, &effect), S_OK);
    EXPECT_FALSE(recording->entered);
    EXPECT_EQ(effect, DROPEFFECT_COPY);

    recording->Release();
}

TEST(FileDropTarget, FilterKeepsOnlySupportedFiles)
{
    std::vector<std::wstring> received;
    FileDropTarget target(
        [&](std::vector<std::wstring> files) { received = std::move(files); },
        [](const std::wstring& path) {
            return path.size() >= 3 && path.compare(path.size() - 3, 3, L".md") == 0;
        });

    FakeDataObject data({L"C:\\notes\\readme.txt", L"C:\\notes\\page.md"});
    DWORD effect = DROPEFFECT_COPY;
    POINTL point = {0, 0};
    EXPECT_EQ(target.DragEnter(&data, 0, point, &effect), S_OK);
    EXPECT_EQ(effect, DROPEFFECT_COPY);

    effect = DROPEFFECT_COPY;
    EXPECT_EQ(target.Drop(&data, 0, point, &effect), S_OK);
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], L"C:\\notes\\page.md");
}

TEST(FileDropTarget, FilterRejectsDragWithoutSupportedFiles)
{
    std::vector<std::wstring> received;
    FileDropTarget target(
        [&](std::vector<std::wstring> files) { received = std::move(files); },
        [](const std::wstring&) { return false; });

    FakeDataObject data({L"C:\\notes\\readme.txt"});
    DWORD effect = DROPEFFECT_COPY;
    POINTL point = {0, 0};
    EXPECT_EQ(target.DragEnter(&data, 0, point, &effect), S_OK);
    EXPECT_EQ(effect, DROPEFFECT_NONE);
    EXPECT_TRUE(received.empty());
}

} // namespace awm::ui
