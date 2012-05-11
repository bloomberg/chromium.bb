// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/host_ui_resource.h"

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

  virtual void Show(remoting::ChromotingHost* host,
                    const ContinueSessionCallback& callback) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  static BOOL CALLBACK DialogProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void EndDialog();
  void SetStrings(const UiStrings& strings);

  remoting::ChromotingHost* host_;
  ContinueSessionCallback callback_;
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
    case WM_CLOSE:
      // Ignore close messages.
      return TRUE;
    case WM_DESTROY:
      // Ensure we don't try to use the HWND anymore.
      hwnd_ = NULL;
      return TRUE;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_CONTINUE_DEFAULT:
          callback_.Run(true);
          ::EndDialog(hwnd, LOWORD(wParam));
          hwnd_ = NULL;
          return TRUE;
        case IDC_CONTINUE_CANCEL:
          callback_.Run(false);
          ::EndDialog(hwnd, LOWORD(wParam));
          hwnd_ = NULL;
          return TRUE;
      }
  }
  return FALSE;
}

void ContinueWindowWin::Show(ChromotingHost* host,
                             const ContinueSessionCallback& callback) {
  host_ = host;
  callback_ = callback;

  CHECK(!hwnd_);
  hwnd_ = CreateDialogParam(g_hModule, MAKEINTRESOURCE(IDD_CONTINUE), NULL,
      (DLGPROC)DialogProc, (LPARAM)this);
  if (!hwnd_) {
    LOG(ERROR) << "Unable to create Disconnect dialog for remoting.";
    return;
  }

  SetStrings(host->ui_strings());
  ShowWindow(hwnd_, SW_SHOW);
}

void ContinueWindowWin::Hide() {
  EndDialog();
}

void ContinueWindowWin::EndDialog() {
  if (hwnd_) {
    ::DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void ContinueWindowWin::SetStrings(const UiStrings& strings) {
  SetWindowText(hwnd_, strings.product_name.c_str());

  HWND hwndMessage = GetDlgItem(hwnd_, IDC_CONTINUE_MESSAGE);
  CHECK(hwndMessage);
  SetWindowText(hwndMessage, strings.continue_prompt.c_str());

  HWND hwndDefault = GetDlgItem(hwnd_, IDC_CONTINUE_DEFAULT);
  CHECK(hwndDefault);
  SetWindowText(hwndDefault, strings.continue_button_text.c_str());

  HWND hwndCancel = GetDlgItem(hwnd_, IDC_CONTINUE_CANCEL);
  CHECK(hwndCancel);
  SetWindowText(hwndCancel, strings.stop_sharing_button_text.c_str());
}

scoped_ptr<ContinueWindow> ContinueWindow::Create() {
  return scoped_ptr<ContinueWindow>(new ContinueWindowWin());
}

}  // namespace remoting
