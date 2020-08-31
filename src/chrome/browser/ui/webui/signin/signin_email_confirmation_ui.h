// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"

class SigninEmailConfirmationUI : public ConstrainedWebDialogUI {
 public:
  explicit SigninEmailConfirmationUI(content::WebUI* web_ui);
  ~SigninEmailConfirmationUI() override;

  // Closes this sign-in email confirmation webUI.
  void Close();

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninEmailConfirmationUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_UI_H_
