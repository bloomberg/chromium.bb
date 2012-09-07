// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

struct Globals {
  LPTHREAD_START_ROUTINE host_main;
  void* host_context;
  HWND core_window;
  HWND host_window;
  HANDLE host_thread;
  DWORD main_thread_id;
} globals;


void ODS(const char* str, LONG_PTR val = 0) {
  char buf[80];
  size_t len = strlen(str);
  if (len > 50) {
    ::OutputDebugStringA("ODS: buffer too long");
    return;
  }

  if (str[0] == '!') {
    // Fatal error.
    DWORD gle = ::GetLastError();
    if (::IsDebuggerPresent())
      __debugbreak();
    wsprintfA(buf, "ODS:fatal %s (%p) gle=0x%x", str, val, gle);
    ::MessageBoxA(NULL, buf, "!!!", MB_OK);
    ::ExitProcess(gle);
  } else {
    // Just information.
    wsprintfA(buf, "ODS:%s (%p)\n", str, val);
    ::OutputDebugStringA(buf);
  }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
                         WPARAM wparam, LPARAM lparam) {
  PAINTSTRUCT ps;
  HDC hdc;
  switch (message) {
    case WM_PAINT:
      hdc = BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      ODS("Metro WM_DESTROY received");
      break;
    default:
      return DefWindowProc(hwnd, message, wparam, lparam);
  }
  return 0;
}

HWND CreateMetroTopLevelWindow() {
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(&__ImageBase);
  WNDCLASSEXW wcex;
  wcex.cbSize = sizeof(wcex);
  wcex.style                    = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc        = WndProc;
        wcex.cbClsExtra         = 0;
        wcex.cbWndExtra         = 0;
        wcex.hInstance          = hInst;
        wcex.hIcon              = 0;
        wcex.hCursor            = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground      = (HBRUSH)(COLOR_INACTIVECAPTION+1);
        wcex.lpszMenuName       = 0;
        wcex.lpszClassName      = L"Windows.UI.Core.CoreWindow";
        wcex.hIconSm            = 0;

  HWND hwnd = ::CreateWindowExW(0,
                                MAKEINTATOM(::RegisterClassExW(&wcex)),
                                L"metro_metro",
                                WS_POPUP,
                                0, 0, 0, 0,
                                NULL, NULL, hInst, NULL);
  return hwnd;
}

DWORD WINAPI HostThread(void*) {
  // The sleeps simulates the delay we have in the actual metro code
  // which takes in account the corewindow being created and some other
  // unknown machinations of metro.
  ODS("Chrome main thread", ::GetCurrentThreadId());
  ::Sleep(30);
  return globals.host_main(globals.host_context);
}

extern "C" __declspec(dllexport)
int InitMetro(LPTHREAD_START_ROUTINE thread_proc, void* context) {
  ODS("InitMetro [Win7 emulation]");
  HWND window = CreateMetroTopLevelWindow();
  if (!window)
    return 1;
  // This magic incatation tells windows that the window is going fullscreen
  // so the taskbar gets out of the wait automatically.
  ::SetWindowPos(window,
                 HWND_TOP,
                 0,0,
                 GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN),
                 SWP_SHOWWINDOW);

  // Ready to start our caller.
  globals.core_window = window;
  globals.host_main = thread_proc;
  globals.host_context = context;
  HANDLE thread = ::CreateThread(NULL, 0, &HostThread, NULL, 0, NULL);

  // Main message loop.
  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return (int) msg.wParam;
}

extern "C" _declspec(dllexport) HWND GetRootWindow() {
  ODS("GetRootWindow", ULONG_PTR(globals.core_window));
  return globals.core_window;
}

extern "C" _declspec(dllexport) void SetFrameWindow(HWND window) {
  ODS("SetFrameWindow", ULONG_PTR(window));
  globals.host_window = window;
}

extern "C" __declspec(dllexport) const wchar_t* GetInitialUrl() {
  return L"";
}

