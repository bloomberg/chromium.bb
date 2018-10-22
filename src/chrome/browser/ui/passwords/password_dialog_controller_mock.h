// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_MOCK_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordDialogControllerMock : public PasswordDialogController {
 public:
  PasswordDialogControllerMock();
  ~PasswordDialogControllerMock() override;

  MOCK_CONST_METHOD0(GetLocalForms, const FormsVector&());
  MOCK_CONST_METHOD0(GetAccoutChooserTitle,
                     std::pair<base::string16, gfx::Range>());
  MOCK_CONST_METHOD0(ShouldShowSignInButton, bool());
  MOCK_CONST_METHOD0(GetAutoSigninPromoTitle, base::string16());
  MOCK_CONST_METHOD0(GetAutoSigninText,
                     std::pair<base::string16, gfx::Range>());
  MOCK_METHOD0(OnSmartLockLinkClicked, void());
  MOCK_METHOD2(OnChooseCredentials, void(
      const autofill::PasswordForm& password_form,
      password_manager::CredentialType credential_type));
  MOCK_METHOD0(OnSignInClicked, void());
  MOCK_METHOD0(OnAutoSigninOK, void());
  MOCK_METHOD0(OnAutoSigninTurnOff, void());
  MOCK_METHOD0(OnCloseDialog, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordDialogControllerMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_MOCK_H_
