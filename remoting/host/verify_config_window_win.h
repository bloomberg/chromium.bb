// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VERIFY_CONFIG_WINDOW_WIN_H
#define REMOTING_HOST_VERIFY_CONFIG_WINDOW_WIN_H

#include <string>

#include "base/callback.h"

namespace remoting {

// TODO(simonmorris): Derive this class from ATL's CDialog.
class VerifyConfigWindowWin {
 public:
   VerifyConfigWindowWin(const std::string& email,
                         const std::string& host_id,
                         const std::string& host_secret_hash);
  ~VerifyConfigWindowWin();

  // Run the dialog modally. Returns true on successful verification.
  bool Run();

 private:
  static BOOL CALLBACK DialogProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void InitDialog();
  void EndDialog();
  bool VerifyHostSecretHash();

  HWND hwnd_;
  const std::string email_;
  const std::string host_id_;
  const std::string host_secret_hash_;

  DISALLOW_COPY_AND_ASSIGN(VerifyConfigWindowWin);
};

}

#endif  // REMOTING_HOST_VERIFY_CONFIG_WINDOW_WIN_H
