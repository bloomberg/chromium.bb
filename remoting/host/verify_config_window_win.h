// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VERIFY_CONFIG_WINDOW_WIN_H
#define REMOTING_HOST_VERIFY_CONFIG_WINDOW_WIN_H

// altbase.h must be included before atlapp.h
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlwin.h>
#include <atluser.h>
#include <string>

#include "base/callback.h"
#include "remoting/host/elevated_controller_resource.h"

namespace remoting {

class VerifyConfigWindowWin : public ATL::CDialogImpl<VerifyConfigWindowWin> {
 public:
  enum { IDD = IDD_VERIFY_CONFIG_DIALOG };

  BEGIN_MSG_MAP_EX(VerifyConfigWindowWin)
    MSG_WM_INITDIALOG(OnInitDialog)
    MSG_WM_CLOSE(OnClose)
    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

  VerifyConfigWindowWin(const std::string& email,
                        const std::string& host_id,
                        const std::string& host_secret_hash);

  void OnCancel(UINT code, int id, HWND control);
  void OnClose();
  LRESULT OnInitDialog(HWND wparam, LPARAM lparam);
  void OnOk(UINT code, int id, HWND control);

 private:
  // Centers the dialog window against the owner window.
  void CenterWindow();
  bool VerifyHostSecretHash();

  CIcon icon_;

  const std::string email_;
  const std::string host_id_;
  const std::string host_secret_hash_;

  DISALLOW_COPY_AND_ASSIGN(VerifyConfigWindowWin);
};

}

#endif  // REMOTING_HOST_VERIFY_CONFIG_WINDOW_WIN_H
