// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/verify_config_window_win.h"

#include <atlbase.h>
#include <windows.h>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "remoting/host/elevated_controller_resource.h"
#include "remoting/host/pin_hash.h"
#include "remoting/protocol/authentication_method.h"

namespace remoting {

VerifyConfigWindowWin::VerifyConfigWindowWin(const std::string& email,
    const std::string& host_id, const std::string& host_secret_hash)
  : hwnd_(NULL),
    email_(email),
    host_id_(host_id),
    host_secret_hash_(host_secret_hash) {
}

VerifyConfigWindowWin::~VerifyConfigWindowWin() {
  EndDialog();
}

bool VerifyConfigWindowWin::Run() {
  // TODO(simonmorris): Provide a handle of a parent window for this dialog.
  return (DialogBoxParam(ATL::_AtlBaseModule.GetModuleInstance(),
                         MAKEINTRESOURCE(IDD_VERIFY_CONFIG_DIALOG),
                         NULL,
                         (DLGPROC)DialogProc,
                         (LPARAM)this) != 0);
}

BOOL CALLBACK VerifyConfigWindowWin::DialogProc(HWND hwnd, UINT msg,
                                                WPARAM wParam, LPARAM lParam) {
  VerifyConfigWindowWin* win = NULL;
  if (msg == WM_INITDIALOG) {
    win = reinterpret_cast<VerifyConfigWindowWin*>(lParam);
    CHECK(win);
    SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)win);
  } else {
    LONG_PTR lp = GetWindowLongPtr(hwnd, DWLP_USER);
    win = reinterpret_cast<VerifyConfigWindowWin*>(lp);
  }
  if (win == NULL)
    return FALSE;
  return win->OnDialogMessage(hwnd, msg, wParam, lParam);
}

BOOL VerifyConfigWindowWin::OnDialogMessage(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
      hwnd_ = hwnd;
      InitDialog();
      return TRUE;
    case WM_DESTROY:
      ::EndDialog(hwnd, 0);
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDOK:
          ::EndDialog(hwnd, VerifyHostSecretHash());
          hwnd_ = NULL;
          return TRUE;
        case IDCANCEL:
          ::EndDialog(hwnd, 0);
          hwnd_ = NULL;
          return TRUE;
      }
  }
  return FALSE;
}

void VerifyConfigWindowWin::InitDialog() {
  // TODO(simonmorris): l10n.
  SetWindowText(hwnd_, L"Chrome Remote Desktop");

  HWND hwndOk = GetDlgItem(hwnd_, IDOK);
  CHECK(hwndOk);
  SetWindowText(hwndOk, L"OK");

  HWND hwndCancel = GetDlgItem(hwnd_, IDCANCEL);
  CHECK(hwndCancel);
  SetWindowText(hwndCancel, L"Cancel");

  HWND hwndMessage = GetDlgItem(hwnd_, IDC_MESSAGE);
  CHECK(hwndMessage);
  SetWindowText(hwndMessage, L"To confirm that your Chrome Remote Desktop "
      L"should be accessible by this account, please enter your PIN below.");

  HWND hwndEmail = GetDlgItem(hwnd_, IDC_EMAIL);
  CHECK(hwndEmail);
  SetWindowText(hwndEmail, UTF8ToUTF16(email_).c_str());

  HWND hwndPin = GetDlgItem(hwnd_, IDC_PIN);
  CHECK(hwndPin);
  SetFocus(hwndPin);
}

void VerifyConfigWindowWin::EndDialog() {
  if (hwnd_) {
    ::EndDialog(hwnd_, 0);
    hwnd_ = NULL;
  }
}

bool VerifyConfigWindowWin::VerifyHostSecretHash() {
  const int kMaxPinLength = 256;
  // TODO(simonmorris): Use ATL's string class, if it's more convenient.
  scoped_array<WCHAR> pinWSTR(new WCHAR[kMaxPinLength]);
  HWND hwndPin = GetDlgItem(hwnd_, IDC_PIN);
  CHECK(hwndPin);
  GetWindowText(hwndPin, pinWSTR.get(), kMaxPinLength);
  std::string pin(UTF16ToUTF8(pinWSTR.get()));
  return VerifyHostPinHash(host_secret_hash_, host_id_, pin);
}

}  // namespace remoting
