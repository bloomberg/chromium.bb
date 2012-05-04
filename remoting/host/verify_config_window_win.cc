// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/verify_config_window_win.h"

#include <atlbase.h>
#include <atlwin.h>
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
  : email_(email),
    host_id_(host_id),
    host_secret_hash_(host_secret_hash) {
}

void VerifyConfigWindowWin::OnCancel(UINT code, int id, HWND control) {
  EndDialog(IDCANCEL);
}

void VerifyConfigWindowWin::OnClose() {
  EndDialog(IDCANCEL);
}

LRESULT VerifyConfigWindowWin::OnInitDialog(HWND wparam, LPARAM lparam) {
  // Set the small window icon.
  if (icon_.LoadIcon(IDD, ::GetSystemMetrics(SM_CXSMICON),
                     ::GetSystemMetrics(SM_CYSMICON)) != NULL) {
    SetIcon(icon_, FALSE);
  }

  CenterWindow();

  // TODO(simonmorris): l10n.
  SetWindowText(L"Chrome Remote Desktop");

  CWindow email_label(GetDlgItem(IDC_EMAIL_LABEL));
  email_label.SetWindowText(L"Account:");

  CWindow pin_label(GetDlgItem(IDC_PIN_LABEL));
  pin_label.SetWindowText(L"PIN:");

  CWindow ok_button(GetDlgItem(IDOK));
  ok_button.SetWindowText(L"Confirm");

  CWindow cancel_button(GetDlgItem(IDCANCEL));
  cancel_button.SetWindowText(L"Cancel");

  CWindow message_text(GetDlgItem(IDC_MESSAGE));
  message_text.SetWindowText(L"Please confirm your account and PIN below to "
                             L"allow access by Chrome Remote Desktop.");

  CWindow email_text(GetDlgItem(IDC_EMAIL));
  email_text.SetWindowText(UTF8ToUTF16(email_).c_str());
  return TRUE;
}

void VerifyConfigWindowWin::OnOk(UINT code, int id, HWND control) {
  if (VerifyHostSecretHash()) {
    EndDialog(IDOK);
  } else {
    EndDialog(IDCANCEL);
  }
}

void VerifyConfigWindowWin::CenterWindow() {
  // Get the window dimensions.
  RECT rect;
  if (!GetWindowRect(&rect)) {
    return;
  }

  // Center against the owner window unless it is minimized or invisible.
  HWND owner = ::GetWindow(m_hWnd, GW_OWNER);
  if (owner != NULL) {
    DWORD style = ::GetWindowLong(owner, GWL_STYLE);
    if ((style & WS_MINIMIZE) != 0 || (style & WS_VISIBLE) == 0) {
      owner = NULL;
    }
  }

  // Make sure that the window will not end up split by a monitor's boundary.
  RECT area_rect;
  if (!::SystemParametersInfo(SPI_GETWORKAREA, NULL, &area_rect, NULL)) {
    return;
  }

  // On a multi-monitor system use the monitor where the owner window is shown.
  RECT owner_rect = area_rect;
  if (owner != NULL && ::GetWindowRect(owner, &owner_rect)) {
    HMONITOR monitor = ::MonitorFromRect(&owner_rect, MONITOR_DEFAULTTONEAREST);
    if (monitor != NULL) {
      MONITORINFO monitor_info = {0};
      monitor_info.cbSize = sizeof(monitor_info);
      if (::GetMonitorInfo(monitor, &monitor_info)) {
        area_rect = monitor_info.rcWork;
      }
    }
  }

  LONG width = rect.right - rect.left;
  LONG height = rect.bottom - rect.top;
  LONG x  = (owner_rect.left + owner_rect.right - width) / 2;
  LONG y = (owner_rect.top + owner_rect.bottom - height) / 2;

  x = std::max(x, area_rect.left);
  x = std::min(x, area_rect.right - width);
  y = std::max(y, area_rect.top);
  y = std::min(y, area_rect.bottom - width);

  SetWindowPos(NULL, x, y, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool VerifyConfigWindowWin::VerifyHostSecretHash() {
  CWindow pin_edit(GetDlgItem(IDC_PIN));

  // Get the PIN length.
  int pin_length = pin_edit.GetWindowTextLength();
  scoped_array<char16> pin(new char16[pin_length + 1]);

  // Get the PIN making sure it is NULL terminated even if an error occurs.
  int result = pin_edit.GetWindowText(pin.get(), pin_length + 1);
  pin[std::min(result, pin_length)] = 0;

  return VerifyHostPinHash(host_secret_hash_, host_id_, UTF16ToUTF8(pin.get()));
}

}  // namespace remoting
