// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/win/start_host_window.h"

#include <atlbase.h>
#include <atlwin.h>
#include <windows.h>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "google_apis/gaia/gaia_urls.h"
#include "remoting/host/service_urls.h"
#include "remoting/host/setup/oauth_helper.h"
#include "remoting/host/setup/pin_validator.h"
#include "remoting/host/setup/win/load_string_from_resource.h"

namespace remoting {

StartHostWindow::StartHostWindow(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : host_starter_(remoting::HostStarter::Create(
          GaiaUrls::GetInstance()->oauth2_token_url(),
          remoting::ServiceUrls::GetInstance()->directory_hosts_url(),
          url_request_context_getter)),
      consent_to_collect_data_(true),
      mem_mgr_(GetProcessHeap()),
      string_mgr_(&mem_mgr_),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_ptr_(weak_ptr_factory_.GetWeakPtr()) {
}

void StartHostWindow::OnCancel(UINT code, int id, HWND control) {
  EndDialog(IDCANCEL);
}

void StartHostWindow::OnClose() {
  EndDialog(IDCANCEL);
}

LRESULT StartHostWindow::OnInitDialog(HWND wparam, LPARAM lparam) {
  // Failure of any of these calls is acceptable.
  SetWindowText(LoadStringFromResource(IDS_TITLE));
  GetDlgItem(IDC_CONSENT).SetWindowText(LoadStringFromResource(IDS_CONSENT));
  CheckDlgButton(IDC_CONSENT, BST_CHECKED);
  return TRUE;
}

void StartHostWindow::OnConsent(UINT code, int id, HWND control) {
  bool checked = (IsDlgButtonChecked(IDC_CONSENT) == BST_CHECKED);
  checked = !checked;
  CheckDlgButton(IDC_CONSENT, checked ? BST_CHECKED : BST_UNCHECKED);
}

void StartHostWindow::OnOk(UINT code, int id, HWND control) {
  host_name_ = GetDlgItemString(IDC_HOST_NAME);
  pin_ = GetDlgItemString(IDC_PIN);
  std::string confirm_pin = GetDlgItemString(IDC_CONFIRM_PIN);
  consent_to_collect_data_ = (IsDlgButtonChecked(IDC_CONSENT) == BST_CHECKED);
  if (pin_ != confirm_pin) {
    MessageBox(LoadStringFromResource(IDS_SAME_PIN),
               LoadStringFromResource(IDS_TITLE),
               MB_ICONEXCLAMATION | MB_OK);
    return;
  }
  if (!IsPinValid(pin_)) {
    MessageBox(LoadStringFromResource(IDS_INVALID_PIN),
               LoadStringFromResource(IDS_TITLE), MB_ICONEXCLAMATION | MB_OK);
    return;
  }
  MessageBox(LoadStringFromResource(IDS_USE_BROWSER),
             LoadStringFromResource(IDS_TITLE), MB_OK);
  auth_code_getter_.GetAuthCode(
      base::Bind(&StartHostWindow::OnAuthCode, weak_ptr_));
}

void StartHostWindow::OnAuthCode(const std::string& auth_code) {
  if (auth_code.empty())
    return;

  host_starter_->StartHost(
      host_name_, pin_, consent_to_collect_data_, auth_code,
      GetDefaultOauthRedirectUrl(),
      base::Bind(&StartHostWindow::OnHostStarted, weak_ptr_));
}

void StartHostWindow::OnHostStarted(remoting::HostStarter::Result result) {
  if (result == remoting::HostStarter::START_COMPLETE) {
    MessageBox(LoadStringFromResource(IDS_HOST_START_SUCCEEDED),
               LoadStringFromResource(IDS_TITLE), MB_OK);
    EndDialog(IDOK);
  } else {
    MessageBox(LoadStringFromResource(IDS_HOST_START_FAILED),
               LoadStringFromResource(IDS_TITLE), MB_ICONEXCLAMATION | MB_OK);
  }
}

std::string StartHostWindow::GetDlgItemString(int id) {
  CSimpleString str(L"", &string_mgr_);
  GetDlgItemText(id, str);
  return UTF16ToUTF8(str.GetString());
}

}  // namespace remoting
