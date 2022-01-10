// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_AUTHENTICATOR_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class AccountId;

namespace chromeos {

class UserContext;

// An interface for objects that will authenticate a Chromium OS user.
// Callbacks will be called on the UI thread:
// 1. On successful authentication, will call consumer_->OnAuthSuccess().
// 2. On failure, will call consumer_->OnAuthFailure().
// 3. On password change, will call consumer_->OnPasswordChangeDetected().
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) Authenticator
    : public base::RefCountedThreadSafe<Authenticator> {
 public:
  explicit Authenticator(AuthStatusConsumer* consumer);

  Authenticator(const Authenticator&) = delete;
  Authenticator& operator=(const Authenticator&) = delete;

  // Given externally authenticated username and password (part of
  // |user_context|), this method attempts to complete authentication process.
  virtual void CompleteLogin(std::unique_ptr<UserContext> user_context) = 0;

  // Given a user credentials in |user_context|,
  // this method attempts to authenticate to login.
  // Must be called on the UI thread.
  virtual void AuthenticateToLogin(
      std::unique_ptr<UserContext> user_context) = 0;

  // Initiates incognito ("browse without signing in") login.
  virtual void LoginOffTheRecord() = 0;

  // Initiates login into the public account identified by |user_context|.
  virtual void LoginAsPublicSession(const UserContext& user_context) = 0;

  // Initiates login into kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // So called Public mount is used to mount cryptohome.
  virtual void LoginAsKioskAccount(const AccountId& app_account_id) = 0;

  // Initiates login into ARC kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // ARC kiosk mode mounts a public cryptohome.
  virtual void LoginAsArcKioskAccount(const AccountId& app_account_id) = 0;

  // Initiates login into web kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // Web kiosk mode mounts a public cryptohome.
  virtual void LoginAsWebKioskAccount(const AccountId& app_account_id) = 0;

  // Notifies caller that login was successful. Must be called on the UI thread.
  virtual void OnAuthSuccess() = 0;

  // Must be called on the UI thread.
  virtual void OnAuthFailure(const AuthFailure& error) = 0;

  // Call these methods on the UI thread.
  // If a password logs the user in online, but cannot be used to
  // mount their cryptohome, we expect that a password change has
  // occurred.
  // Call this method to migrate the user's encrypted data
  // forward to use their new password. |old_password| is the password
  // their data was last encrypted with.
  virtual void RecoverEncryptedData(const std::string& old_password) = 0;

  // Call this method to erase the user's encrypted data
  // and create a new cryptohome.
  virtual void ResyncEncryptedData() = 0;

  // Sets consumer explicitly.
  void SetConsumer(AuthStatusConsumer* consumer);

 protected:
  virtual ~Authenticator();

  AuthStatusConsumer* consumer_;

 private:
  friend class base::RefCountedThreadSafe<Authenticator>;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::Authenticator;
}

#endif  // CHROMEOS_LOGIN_AUTH_AUTHENTICATOR_H_
