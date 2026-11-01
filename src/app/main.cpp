// ActiveXWebMark — application entry point.

#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlwin.h>

#include <stdlib.h>

#include <shellapi.h>
#include <string>

#include "mainwindow.h"

CAppModule _Module;

namespace {

// Extracts an optional file path from the command line (first positional arg).
std::wstring fileFromCommandLine()
{
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return {};
    }
    std::wstring file = (argc > 1) ? std::wstring(argv[1]) : std::wstring();
    ::LocalFree(argv);
    return file;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int nCmdShow)
{
    // OleInitialize (STA) is required for hosting the IE WebBrowser (OLE) control.
    const HRESULT hr = ::OleInitialize(nullptr);
    if (FAILED(hr)) {
        return static_cast<int>(hr);
    }

    _Module.Init(nullptr, hInstance);

    int nRet = 0;
    {
        CMessageLoop theLoop;
        _Module.AddMessageLoop(&theLoop);

        awm::MainWindow wnd;
        wnd.Create(nullptr, CWindow::rcDefault, L"ActiveXWebMark", WS_OVERLAPPEDWINDOW);
        theLoop.AddMessageFilter(&wnd);

        const std::wstring file = fileFromCommandLine();

        wnd.ShowWindow(nCmdShow);
        wnd.UpdateWindow();

        if (!file.empty()) {
            wnd.OpenFile(file);
        }

        nRet = theLoop.Run();

        theLoop.RemoveMessageFilter(&wnd);
        _Module.RemoveMessageLoop();
    }

    _Module.Term();
    ::OleUninitialize();
    return nRet;
}
