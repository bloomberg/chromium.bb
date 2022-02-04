// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_LOGIN_UI_SIGNIN_UI_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_SIGNIN_UI_H_

#include "ash/components/login/auth/user_context.h"
#include "base/callback.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "components/account_id/account_id.h"

namespace ash {

enum class SigninError {
  kCaptivePortalError,
  kGoogleAccountNotAllowed,
  kOwnerRequired,
  kTpmUpdateRequired,
  kKnownUserFailedNetworkNotConnected,
  kNewUserFailedNetworkNotConnected,
  kNewUserFailedNetworkConnected,
  kKnownUserFailedNetworkConnected,
  kOwnerKeyLost,
  kChallengeResponseAuthMultipleClientCerts,
  kChallengeResponseAuthInvalidClientCert,
  kCookieWaitTimeout,
  kFailedToFetchSamlRedirect,
  kActiveDirectoryNetworkProblem,
  kActiveDirectoryNotSupportedEncryption,
  kActiveDirectoryUnknownError,
};

// This class represents an interface between code that performs sign-in
// operations and code that handles sign-in UI. It is used to encapsulate UI
// implementation details and declare the required set of parameters that need
// to be set for particular UI.
class SigninUI {
 public:
  SigninUI() = default;
  virtual ~SigninUI() = default;
  SigninUI(const SigninUI&) = delete;
  SigninUI& operator=(const SigninUI&) = delete;

  // Starts user onboarding after successful sign-in for new users.
  virtual void StartUserOnboarding() = 0;
  // Resumes user onboarding after successful sign-in for returning users.
  virtual void ResumeUserOnboarding(OobeScreenId screen_id) = 0;
  // Show UI for management transition flow.
  virtual void StartManagementTransition() = 0;
  // Show additional terms of service on login.
  virtual void ShowTosForExistingUser() = 0;

  virtual void StartEncryptionMigration(
      const UserContext& user_context,
      EncryptionMigrationMode migration_mode,
      base::OnceCallback<void(const UserContext&)> skip_migration_callback) = 0;

  // Might store authentication data so that additional auth factors can be
  // added during user onboarding.
  virtual void SetAuthSessionForOnboarding(const UserContext& user_context) = 0;

  // Clears authentication data that were stored for user onboarding.
  virtual void ClearOnboardingAuthSession() = 0;

  // Show password changed dialog. If `show_password_error` is true, user
  // already tried to enter old password but it turned out to be incorrect.
  virtual void ShowPasswordChangedDialog(const AccountId& account_id,
                                         bool password_incorrect) = 0;

  virtual void ShowSigninError(SigninError error,
                               const std::string& details) = 0;

  // Show the browser data migration UI and start the migration.
  virtual void StartBrowserDataMigration() = 0;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::SigninError;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_SIGNIN_UI_H_
