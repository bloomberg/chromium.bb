// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_WIN_START_HOST_WINDOW_H_
#define REMOTING_HOST_SETUP_WIN_START_HOST_WINDOW_H_

// altbase.h must be included before atlapp.h
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlstr.h>
#include <atluser.h>
#include <atlwin.h>
#include <string>

#include "base/basictypes.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/setup/win/auth_code_getter.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/win/host_configurer_resource.h"

namespace remoting {

// A dialog box that lets the user register and start a host.
class StartHostWindow : public ATL::CDialogImpl<StartHostWindow> {
 public:
  enum { IDD = IDD_START_HOST };

  BEGIN_MSG_MAP_EX(StartHostWindowWin)
    MSG_WM_INITDIALOG(OnInitDialog)
    MSG_WM_CLOSE(OnClose)
    COMMAND_ID_HANDLER_EX(IDC_CONSENT, OnConsent)
    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

  StartHostWindow(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

 private:
  void OnCancel(UINT code, int id, HWND control);
  void OnClose();
  LRESULT OnInitDialog(HWND wparam, LPARAM lparam);
  void OnConsent(UINT code, int id, HWND control);
  void OnOk(UINT code, int id, HWND control);
  void OnAuthCode(const std::string& auth_code);
  void OnHostStarted(remoting::HostStarter::Result result);

  // Gets the text associated with an item in this dialog box.
  std::string GetDlgItemString(int id);

  remoting::AuthCodeGetter auth_code_getter_;
  scoped_ptr<remoting::HostStarter> host_starter_;

  // Data read from widgets in the dialog box.
  std::string host_name_;
  std::string pin_;
  bool consent_to_collect_data_;

  // A string manager that lets us use CSimpleString.
  CWin32Heap mem_mgr_;
  CAtlStringMgr string_mgr_;

  // WeakPtr used to avoid AuthCodeGetter or HostStarter callbacks accessing
  // this dialog box after it's closed.
  base::WeakPtrFactory<StartHostWindow> weak_ptr_factory_;
  base::WeakPtr<StartHostWindow> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(StartHostWindow);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_WIN_START_HOST_WINDOW_H_
