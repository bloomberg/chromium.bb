// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
// TODO(wez): The DisconnectWindow isn't plugin-specific, so shouldn't have
// a dependency on the plugin's resource header.
#include "remoting/host/plugin/host_plugin_resource.h"

// TODO(garykac): Lots of duplicated code in this file and
// disconnect_window_win.cc. These global floating windows are temporary so
// they should be deleted soon. If we need to expand this then we should
// create a class with the shared code.

// HMODULE from DllMain/WinMain. This is needed to find our dialog resource.
// This is defined in:
//   Plugin: host_plugin.cc
//   SimpleHost: simple_host_process.cc
extern HMODULE g_hModule;

namespace remoting {

class ContinueWindowWin : public ContinueWindow {
 public:
  ContinueWindowWin();
  virtual ~ContinueWindowWin();

  virtual void Show(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  static BOOL CALLBACK DialogProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void EndDialog();

  remoting::ChromotingHost* host_;
  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(ContinueWindowWin);
};

ContinueWindowWin::ContinueWindowWin()
    : host_(NULL),
      hwnd_(NULL) {
}

ContinueWindowWin::~ContinueWindowWin() {
  EndDialog();
}

BOOL CALLBACK ContinueWindowWin::DialogProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam) {
  ContinueWindowWin* win = NULL;
  if (msg == WM_INITDIALOG) {
    win = reinterpret_cast<ContinueWindowWin*>(lParam);
    CHECK(win);
    SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)win);
  } else {
    LONG_PTR lp = GetWindowLongPtr(hwnd, DWLP_USER);
    win = reinterpret_cast<ContinueWindowWin*>(lp);
  }
  if (win == NULL)
    return FALSE;
  return win->OnDialogMessage(hwnd, msg, wParam, lParam);
}

BOOL ContinueWindowWin::OnDialogMessage(HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
      return TRUE;
    case WM_CLOSE:
      // Ignore close messages.
      return TRUE;
    case WM_DESTROY:
      // Ensure we don't try to use the HWND anymore.
      hwnd_ = NULL;
      return TRUE;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_CONTINUE:
          {
            CHECK(host_);
            host_->PauseSession(false);
            ::EndDialog(hwnd, LOWORD(wParam));
            hwnd_ = NULL;
          }
          return TRUE;
      }
  }
  return FALSE;
}

void ContinueWindowWin::Show(ChromotingHost* host) {
  host_ = host;

  CHECK(!hwnd_);
  hwnd_ = CreateDialogParam(g_hModule, MAKEINTRESOURCE(IDD_CONTINUE), NULL,
      (DLGPROC)DialogProc, (LPARAM)this);
  if (!hwnd_) {
    LOG(ERROR) << "Unable to create Disconnect dialog for remoting.";
    return;
  }

  ShowWindow(hwnd_, SW_SHOW);
}

void ContinueWindowWin::Hide() {
  EndDialog();
}

void ContinueWindowWin::EndDialog() {
  if (hwnd_) {
    ::EndDialog(hwnd_, 0);
    hwnd_ = NULL;
  }
}

ContinueWindow* ContinueWindow::Create() {
  return new ContinueWindowWin();
}

}  // namespace remoting
