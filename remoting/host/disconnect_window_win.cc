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

// TODO(garykac): Lots of duplicated code in this file and
// continue_window_win.cc. These global floating windows are temporary so
// they should be deleted soon. If we need to expand this then we should
// create a class with the shared code.

// HMODULE from DllMain/WinMain. This is needed to find our dialog resource.
// This is defined in:
//   Plugin: host_plugin.cc
//   SimpleHost: simple_host_process.cc
extern HMODULE g_hModule;

const int DISCONNECT_HOTKEY_ID = 1000;

namespace remoting {

class DisconnectWindowWin : public DisconnectWindow {
 public:
  DisconnectWindowWin();
  virtual ~DisconnectWindowWin();

  virtual void Show(remoting::ChromotingHost* host,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

private:
  static BOOL CALLBACK DialogProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void ShutdownHost();
  void SetDisconnectButtonText(HWND hwnd);
  void EndDialog(int result);

  remoting::ChromotingHost* host_;
  std::string username_;
  HWND hwnd_;
  bool has_hotkey_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowWin);
};

DisconnectWindowWin::DisconnectWindowWin()
    : host_(NULL),
      username_(""),
      hwnd_(NULL),
      has_hotkey_(false) {
}

DisconnectWindowWin::~DisconnectWindowWin() {
  EndDialog(0);
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
        // Update UI string placeholders with actual strings.
        std::wstring w_title = UTF8ToWide(kTitle);
        SetWindowText(hwnd, w_title.c_str());

        SetDisconnectButtonText(hwnd);

        HWND hwndSharingWith = GetDlgItem(hwnd, IDC_DISCONNECT_SHARINGWITH);
        CHECK(hwndSharingWith);
        std::wstring w_sharing = UTF8ToWide(kSharingWith);
        SetWindowText(hwndSharingWith, w_sharing.c_str());

        // Update username in dialog.
        HWND hwndUsername = GetDlgItem(hwnd, IDC_DISCONNECT_USERNAME);
        CHECK(hwndUsername);
        std::wstring w_username = UTF8ToWide(username_);
        SetWindowText(hwndUsername, w_username.c_str());
      }
      return TRUE;
    case WM_HOTKEY:
      {
        ShutdownHost();
        EndDialog(0);
      }
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
        case IDC_DISCONNECT:
          {
            ShutdownHost();
            EndDialog(LOWORD(wParam));
          }
          return TRUE;
      }
  }
  return FALSE;
}

void DisconnectWindowWin::Show(ChromotingHost* host,
                               const std::string& username) {
  host_ = host;
  username_ = username;

  CHECK(!hwnd_);
  hwnd_ = CreateDialogParam(g_hModule, MAKEINTRESOURCE(IDD_DISCONNECT), NULL,
      (DLGPROC)DialogProc, (LPARAM)this);
  if (!hwnd_) {
    LOG(ERROR) << "Unable to create Disconnect dialog for remoting.";
    return;
  }

  ShowWindow(hwnd_, SW_SHOW);

  // Set up handler for Ctrl-Alt-Esc shortcut.
  if (!has_hotkey_ && RegisterHotKey(hwnd_, DISCONNECT_HOTKEY_ID,
                                     MOD_ALT | MOD_CONTROL, VK_ESCAPE)) {
    has_hotkey_ = true;
  }
  SetDisconnectButtonText(hwnd_);
}

void DisconnectWindowWin::ShutdownHost() {
  CHECK(host_);
  host_->Shutdown(NULL);
}

void DisconnectWindowWin::SetDisconnectButtonText(HWND hwnd) {
  HWND hwndButton = GetDlgItem(hwnd, IDC_DISCONNECT);
  CHECK(hwndButton);
  std::wstring w_button = UTF8ToWide(kDisconnectButton);
  if (has_hotkey_)
    w_button += UTF8ToWide(kDisconnectKeysWin);
  SetWindowText(hwndButton, w_button.c_str());
}

void DisconnectWindowWin::Hide() {
  EndDialog(0);
}

void DisconnectWindowWin::EndDialog(int result) {
  if (has_hotkey_) {
    UnregisterHotKey(hwnd_, DISCONNECT_HOTKEY_ID);
    has_hotkey_ = false;
  }

  if (hwnd_) {
    ::EndDialog(hwnd_, result);
    hwnd_ = NULL;
  }
}

DisconnectWindow* DisconnectWindow::Create() {
  return new DisconnectWindowWin;
}

}  // namespace remoting
