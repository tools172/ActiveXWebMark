#include "mainwindow.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>

#include "scintillaeditor.h"
#include "highlight/pmhhighlighter.h"
#include "markdown/cmarkengine.h"
#include "util/stringutil.h"

namespace awm {

namespace {

// Wraps rendered body HTML in a minimal document with base styles.
std::string WrapHtml(const std::string& body)
{
    return
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><base target=\"_blank\">"
        "<style>"
        "body{font-family:'Segoe UI',sans-serif;max-width:900px;margin:24px auto;"
        "padding:0 24px;line-height:1.6;color:#24292f;}"
        "h1,h2,h3,h4{border-bottom:1px solid #eee;padding-bottom:.3em;}"
        "code{background:#f6f8fa;padding:2px 4px;border-radius:3px;font-family:Consolas,monospace;}"
        "pre{background:#f6f8fa;padding:12px;overflow:auto;border-radius:6px;}"
        "pre code{background:none;padding:0;}"
        "blockquote{border-left:4px solid #d0d7de;margin:0;padding:0 1em;color:#57606a;}"
        "table{border-collapse:collapse;}td,th{border:1px solid #d0d7de;padding:6px 12px;}"
        "img{max-width:100%;}"
        "</style></head><body>" + body + "</body></html>";
}

} // namespace

MainWindow::MainWindow()
    : registry_(DocumentFormatRegistry::createDefault())
    , engine_(std::make_unique<CmarkEngine>())
    , highlighter_(std::make_unique<PmhSyntaxHighlighter>())
{
}

BOOL MainWindow::PreTranslateMessage(MSG* pMsg)
{
    // Forward keyboard messages to the preview control so it can handle
    // accelerators such as Ctrl+C (copy selected text).
    if (preview_ != nullptr && preview_->TranslateAccelerator(pMsg)) {
        return TRUE;
    }
    return FALSE;
}

void MainWindow::CreateMenuBar()
{
    HMENU fileMenu = ::CreatePopupMenu();
    ::AppendMenuW(fileMenu, MF_STRING, kCmdFileOpen, L"打开(&O)...\tCtrl+O");
    ::AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(fileMenu, MF_STRING, kCmdAppExit, L"退出(&X)");

    HMENU viewMenu = ::CreatePopupMenu();
    ::AppendMenuW(viewMenu, MF_STRING, kCmdViewEditor, L"显示编辑器(&E)\tCtrl+E");

    HMENU menu = ::CreateMenu();
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"文件(&F)");
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"视图(&V)");
    ::SetMenu(*this, menu);
}

LRESULT MainWindow::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
    RECT rc = {0, 0, 0, 0};

    navTree_.Create(*this, rc, nullptr,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT |
                        TVS_HASBUTTONS,
                    WS_EX_CLIENTEDGE, kNavTreeId);

    preview_ = CreatePreviewView(PreviewEngine::Ie);
    editor_ = std::make_unique<ScintillaEditor>();

    preview_->Create(*this, rc);
    editor_->Create(*this, rc);
    editor_->Show(false); // preview-first: editor hidden by default.

    // Link external clicks to the system browser.
    PreviewCallbacks callbacks;
    callbacks.onLink = [](const std::string& url) {
        ::ShellExecuteW(nullptr, L"open", utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    };
    preview_->SetCallbacks(std::move(callbacks));

    // Re-render the preview when the editor text changes.
    editor_->SetChangeCallback([this]() {
        const std::string source = editor_->GetText();
        const RenderResult result = engine_->render(source, RenderOptions{});
        preview_->SetHtml(WrapHtml(result.bodyHtml), "");
    });

    // Set the menu last: SetMenu resizes the client area (WM_SIZE), which must
    // only run once the preview/editor children exist.
    CreateMenuBar();

    Layout();
    return 0;
}

LRESULT MainWindow::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    editor_.reset();
    preview_.reset();
    ::PostQuitMessage(0);
    return 0;
}

LRESULT MainWindow::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    Layout();
    return 0;
}

LRESULT MainWindow::OnFileOpen(WORD, WORD, HWND, BOOL&)
{
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = *this;
    ofn.lpstrFilter = L"Markdown 文件 (*.md;*.markdown;*.mdown)\0*.md;*.markdown;*.mdown\0"
                      L"所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (::GetOpenFileNameW(&ofn)) {
        OpenFile(buffer);
    }
    return 0;
}

LRESULT MainWindow::OnViewEditor(WORD, WORD, HWND, BOOL&)
{
    editorVisible_ = !editorVisible_;
    editor_->Show(editorVisible_);
    Layout();
    return 0;
}

LRESULT MainWindow::OnAppExit(WORD, WORD, HWND, BOOL&)
{
    DestroyWindow();
    return 0;
}

LRESULT MainWindow::OnNavSelChanged(int, LPNMHDR, BOOL&)
{
    const HTREEITEM item = navTree_.GetSelectedItem();
    if (item != nullptr) {
        NavigateTo(static_cast<size_t>(navTree_.GetItemData(item)));
    }
    return 0;
}

void MainWindow::OpenFile(const std::wstring& path)
{
    std::error_code ec;
    auto doc = registry_.load(path, ec);
    if (!doc) {
        const std::string msg = "无法打开该文件:\n" + wideToUtf8(path) + "\n错误码: " + ec.message();
        ::MessageBoxW(*this, utf8ToWide(msg).c_str(), L"ActiveXWebMark", MB_ICONERROR);
        return;
    }

    document_ = std::move(doc);
    ::SetWindowTextW(*this, (utf8ToWide(document_->title()) + L" - ActiveXWebMark").c_str());
    RenderDocument();
}

void MainWindow::Layout()
{
    RECT client;
    GetClientRect(&client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    const int navWidth = std::min(300, width / 4);
    if (navTree_.IsWindow()) {
        navTree_.MoveWindow(0, 0, navWidth, height, TRUE);
    }

    if (preview_ == nullptr) {
        return;
    }

    const int rightLeft = navWidth;
    if (editorVisible_ && editor_ != nullptr) {
        const int half = height / 2;
        preview_->SetBounds(RECT{rightLeft, 0, width, half});
        editor_->SetBounds(RECT{rightLeft, half, width, height});
    } else {
        preview_->SetBounds(RECT{rightLeft, 0, width, height});
    }
}

void MainWindow::BuildNavTree(const Outline& outline)
{
    navTree_.DeleteAllItems();
    for (size_t i = 0; i < outline.size(); ++i) {
        std::wstring text;
        if (outline[i].level > 1) {
            text.assign(static_cast<size_t>((outline[i].level - 1) * 2), L' ');
        }
        text += utf8ToWide(outline[i].text);

        const HTREEITEM item = navTree_.InsertItem(text.c_str(), TVI_ROOT, TVI_LAST);
        navTree_.SetItemData(item, static_cast<DWORD_PTR>(i));
    }
}

void MainWindow::RenderDocument()
{
    if (document_ == nullptr) {
        return;
    }

    const RenderResult result = engine_->render(document_->markdownSource(), RenderOptions{});
    currentOutline_ = result.outline;
    preview_->SetHtml(WrapHtml(result.bodyHtml), "");

    editor_->SetText(document_->markdownSource());
    editor_->ApplyHighlighting(highlighter_->highlight(document_->markdownSource()));

    BuildNavTree(currentOutline_);
}

void MainWindow::NavigateTo(size_t outlineIndex)
{
    if (outlineIndex >= currentOutline_.size()) {
        return;
    }

    const OutlineItem& heading = currentOutline_[outlineIndex];
    preview_->ScrollToAnchor(heading.anchor);
    editor_->GotoLine(heading.line);
}

} // namespace awm
