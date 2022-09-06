// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_MOCK_SIGNIN_UI_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_MOCK_SIGNIN_UI_H_

#include <memory>
#include "ash/components/login/auth/user_context.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "components/login/base_screen_handler_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockSigninUI : public SigninUI {
 public:
  MockSigninUI();
  ~MockSigninUI() override;
  MockSigninUI(const MockSigninUI&) = delete;
  MockSigninUI& operator=(const SigninUI&) = delete;

  MOCK_METHOD(void, StartUserOnboarding, (), (override));
  MOCK_METHOD(void, ResumeUserOnboarding, (OobeScreenId), (override));
  MOCK_METHOD(void, StartManagementTransition, (), (override));
  MOCK_METHOD(void, ShowTosForExistingUser, (), (override));
  MOCK_METHOD(void, ShowNewTermsForFlexUsers, (), (override));
  MOCK_METHOD(void,
              StartEncryptionMigration,
              (const UserContext&,
               EncryptionMigrationMode,
               base::OnceCallback<void(const UserContext&)>),
              (override));
  MOCK_METHOD(void,
              SetAuthSessionForOnboarding,
              (const UserContext&),
              (override));
  MOCK_METHOD(void, ClearOnboardingAuthSession, (), (override));
  MOCK_METHOD(void,
              ShowPasswordChangedDialog,
              (const AccountId&, bool),
              (override));
  MOCK_METHOD(void,
              ShowSigninError,
              (SigninError, const std::string&),
              (override));
  MOCK_METHOD(void, StartBrowserDataMigration, (), (override));
  MOCK_METHOD(void,
              SAMLConfirmPassword,
              (::login::StringList, std::unique_ptr<UserContext>),
              (override));
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockSigninUI;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_MOCK_SIGNIN_UI_H_
