// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "remoting/host/chromoting_host.h"
// TODO(wez): The DisconnectWindow isn't plugin-specific, so shouldn't have
// a dependency on the plugin's resource header.
#include "remoting/host/plugin/host_plugin_resource.h"

// HMODULE from DllMain/WinMain. This is needed to find our dialog resource.
// This is defined in:
//   Plugin: host_plugin.cc
//   SimpleHost: simple_host_process.cc
extern HMODULE g_hModule;

namespace {

class DisconnectWindowWin : public remoting::DisconnectWindow {
 public:
  DisconnectWindowWin();
  virtual void Show(remoting::ChromotingHost* host,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

  static BOOL CALLBACK DialogProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
  remoting::ChromotingHost* host_;
  std::string username_;
  HWND hwnd_;

  // A "random" key from the tickcount that is used to validate the WM_USER
  // message sent to end the dialog. This check is used to help protect
  // against someone sending (WM_APP,0,0L) to close the dialog.
  DWORD key_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowWin);
};

DisconnectWindowWin::DisconnectWindowWin()
    : host_(NULL),
      username_(""),
      hwnd_(NULL),
      key_(0L) {
}

BOOL CALLBACK DisconnectWindowWin::DialogProc(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam) {
  DisconnectWindowWin* win = NULL;
  if (msg == WM_INITDIALOG) {
    win = reinterpret_cast<DisconnectWindowWin*>(lParam);
    CHECK(win);
    SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)win);
  } else {
    LONG_PTR lp = GetWindowLongPtr(hwnd, DWLP_USER);
    win = reinterpret_cast<DisconnectWindowWin*>(lp);
  }
  if (win == NULL)
    return FALSE;
  return win->OnDialogMessage(hwnd, msg, wParam, lParam);
}

BOOL DisconnectWindowWin::OnDialogMessage(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
      {
        // Update username in dialog.
        HWND hwndUsername = GetDlgItem(hwnd, IDC_USERNAME);
        CHECK(hwndUsername);
        std::wstring w_username = UTF8ToWide(username_);
        SetWindowText(hwndUsername, w_username.c_str());
      }
      return TRUE;
    case WM_CLOSE:
      // Ignore close messages.
      return TRUE;
    case WM_DESTROY:
      // Ensure we don't try to use the HWND anymore.
      hwnd_ = NULL;
      return TRUE;
    case WM_APP:
      if (key_ == static_cast<DWORD>(lParam)) {
        EndDialog(hwnd, LOWORD(wParam));
        hwnd_ = NULL;
      }
      return TRUE;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_DISCONNECT:
          {
            CHECK(host_);
            host_->Shutdown(NULL);
            EndDialog(hwnd, LOWORD(wParam));
            hwnd_ = NULL;
          }
          return TRUE;
      }
  }
  return FALSE;
}

void DisconnectWindowWin::Show(remoting::ChromotingHost* host,
                               const std::string& username) {
  host_ = host;
  username_ = username;
  // Get a "random" value that we can use to prevent someone from sending a
  // simple (WM_APP,0,0L) message to our window to close it.
  key_ = GetTickCount();

  CHECK(!hwnd_);
  hwnd_ = CreateDialogParam(g_hModule, MAKEINTRESOURCE(IDD_DISCONNECT), NULL,
      (DLGPROC)DialogProc, (LPARAM)this);
  if (!hwnd_) {
    LOG(ERROR) << "Unable to create Disconnect dialog for remoting.";
    return;
  }

  ShowWindow(hwnd_, SW_SHOW);
  // TODO(garykac): Remove this UpdateWindow() call once threading issues are
  // resolved and it's no longer needed.
  UpdateWindow(hwnd_);
}

void DisconnectWindowWin::Hide() {
  if (hwnd_) {
    SendMessage(hwnd_, WM_APP, 0, (LPARAM)key_);
  }
}

}  // namespace

remoting::DisconnectWindow* remoting::DisconnectWindow::Create() {
  return new DisconnectWindowWin;
}
