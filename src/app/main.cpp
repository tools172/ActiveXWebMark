// ActiveXWebMark — application entry point.

#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlwin.h>

#include <commctrl.h>
#include <gdiplus.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "mainwindow.h"

// Activate Common Controls v6 so the native tree/list/dialogs use the modern
// visual styles and TaskDialog is available. The linker merges this into the
// generated application manifest, so no .rc/.manifest file is needed.
#pragma comment(linker, \
                "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
                "version='6.0.0.0' processorArchitecture='*' " \
                "publicKeyToken='6595b64144ccf1df' language='*'\"")

CAppModule _Module;

namespace {

// Collects every positional command-line argument as a file to open.
std::vector<std::wstring> filesFromCommandLine()
{
    std::vector<std::wstring> files;
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return files;
    }

    files.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        files.emplace_back(argv[i]);
    }
    ::LocalFree(argv);
    return files;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/,
                   int nCmdShow)
{
    // Per-monitor DPI awareness must be set before any window is created.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_WIN95_CLASSES | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES |
                           ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx(&commonControls);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        return 1;
    }

    // OleInitialize (STA) is required for hosting the IE WebBrowser (OLE) control.
    const HRESULT hr = ::OleInitialize(nullptr);
    if (FAILED(hr)) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return static_cast<int>(hr);
    }

    _Module.Init(nullptr, hInstance);

    int nRet = 0;
    {
        CMessageLoop theLoop;
        _Module.AddMessageLoop(&theLoop);

        awm::MainWindow wnd;
        // WS_CLIPCHILDREN is essential: MainWindow::OnPaint double-buffers and
        // blits the whole client. Without this style the background blit paints
        // over the view (and its native panes) every time the frame is
        // invalidated by Layout(), leaving the nav tree blank.
        if (wnd.Create(nullptr, CWindow::rcDefault, L"ActiveXWebMark",
                       WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) != nullptr) {
            theLoop.AddMessageFilter(&wnd);

            const std::vector<std::wstring> files = filesFromCommandLine();
            wnd.ShowWindow(wnd.startMaximized() ? SW_SHOWMAXIMIZED : nCmdShow);
            wnd.UpdateWindow();

            if (!files.empty()) {
                wnd.OpenFiles(files);
            }

            nRet = theLoop.Run();
            theLoop.RemoveMessageFilter(&wnd);
        } else {
            nRet = 1;
        }

        _Module.RemoveMessageLoop();
    }

    _Module.Term();
    ::OleUninitialize();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return nRet;
}
