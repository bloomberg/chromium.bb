// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_
#define ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_

#include "ash/components/login/auth/auth_factor_editor.h"
#include "ash/components/login/auth/auth_performer.h"
#include "ash/components/login/auth/authenticator.h"
#include "ash/components/login/auth/mount_performer.h"
#include "ash/components/login/auth/safe_mode_delegate.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AuthFailure;

namespace ash {

class AuthStatusConsumer;

// Authenticator that authenticates user against ChromeOS cryptohome using
// AuthSession API.
// Parallel authentication attempts are not possible, this is guarded by
// resetting all callbacks bound to weak pointers.
// Generic flow for all authentication attempts:
// * Initialize AuthSession (and learn if user exists)
//   * For existing users:
//     * Transform keys if necessary
//     * Authenticate AuthSession
//     * Mount directory
//     * (Safe mode) Check ownership
//   * For new users:
//     * Transform keys if necessary
//     * Add user credentials
//     * Authenticate session with same credentials
//     * Mount home directory (with empty create request)

// There are several points where flows are customized:
//   * Additional configuration of StartAuthSessionRequest
//   * Additional configuration of MountRequest
//   * Customized error handling
//     (e.g. different "default" error for different user types)
//   * Different ways to hash plain text key
//   * Different ways to create crytohome key from key in UserContext

class COMPONENT_EXPORT(ASH_LOGIN_AUTH) AuthSessionAuthenticator
    : public Authenticator {
 public:
  AuthSessionAuthenticator(AuthStatusConsumer* consumer,
                           std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
                           bool is_ephemeral_mount_enforced);

  // Authenticator overrides.
  void CompleteLogin(std::unique_ptr<UserContext> user_context) override;

  void AuthenticateToLogin(std::unique_ptr<UserContext> user_context) override;
  void LoginOffTheRecord() override;
  void LoginAsPublicSession(const UserContext& user_context) override;
  void LoginAsKioskAccount(const AccountId& app_account_id) override;
  void LoginAsArcKioskAccount(const AccountId& app_account_id) override;
  void LoginAsWebKioskAccount(const AccountId& app_account_id) override;
  void OnAuthSuccess() override;
  void OnAuthFailure(const AuthFailure& error) override;
  void RecoverEncryptedData(std::unique_ptr<UserContext> user_context,
                            const std::string& old_password) override;
  void ResyncEncryptedData(std::unique_ptr<UserContext> user_context) override;

 protected:
  ~AuthSessionAuthenticator() override;

 private:
  // Callbacks that handles auth session started for particular login flows.
  // |user_exists| indicates if cryptohome actually exists on the disk,
  // |context| at this point would contain authsession and meta info describing
  // the keys.
  void DoLoginAsPublicSession(bool user_exists,
                              std::unique_ptr<UserContext> context,
                              absl::optional<CryptohomeError> error);
  void DoLoginAsKiosk(bool user_exists,
                      std::unique_ptr<UserContext> context,
                      absl::optional<CryptohomeError> error);
  void DoLoginAsExistingUser(bool user_exists,
                             std::unique_ptr<UserContext> context,
                             absl::optional<CryptohomeError> error);
  void DoCompleteLogin(bool user_exists,
                       std::unique_ptr<UserContext> context,
                       absl::optional<CryptohomeError> error);

  // Common part of login logic shared by user creation flow and flow when
  // user have changed password elsewhere and decides to re-create cryptohome.
  void CompleteLoginImpl(std::unique_ptr<UserContext> user_context);
  void LoginAsKioskImpl(const AccountId& app_account_id,
                        user_manager::UserType user_type,
                        bool force_dircrypto);

  void PrepareForNewAttempt(const std::string& method_id,
                            const std::string& long_desc);

  // Simple callback that notifies about mount success / failure.
  void NotifyAuthSuccess(std::unique_ptr<UserContext> context);
  void NotifyGuestSuccess(std::unique_ptr<UserContext> context);
  void NotifyFailure(AuthFailure::FailureReason reason,
                     std::unique_ptr<UserContext> context);

  // Handles errors specific to authenticating existing users with the password:
  //   if password is known to be correct (e.g. it comes from online auth flow),
  //   special error code would be raised in case of "incorrect password" to
  //   indicate a need to replace password.
  // Other errors are handled by `fallback`.
  void HandlePasswordChangeDetected(AuthErrorCallback fallback,
                                    std::unique_ptr<UserContext> context,
                                    CryptohomeError error);
  // Handles errors specific to the encryption migration:
  //   if the encryption migration is required, triggers OnOldEncryptionDetected
  //   method on the consumer.
  // Other errors are handled by `fallback`.
  void HandleMigrationRequired(AuthErrorCallback fallback,
                               std::unique_ptr<UserContext> context,
                               CryptohomeError error);

  bool ResolveCryptohomeError(AuthFailure::FailureReason default_error,
                              CryptohomeError& error);
  // Generic error handler, can be used as ErrorHandlingCallback when first
  // parameter is bound to a user type-specific failure reason.
  void ProcessCryptohomeError(AuthFailure::FailureReason default_reason,
                              std::unique_ptr<UserContext> user_context,
                              CryptohomeError error);
  // Check used for existing regular users - in safe mode would check
  // if home directory contains valid owner key. If key is not found,
  // would unmount directory and notify failure.
  void CheckOwnershipOperation(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);
  void OnSafeModeOwnershipCheck(std::unique_ptr<UserContext> context,
                                AuthOperationCallback callback,
                                bool is_owner);
  void OnUnmountForNonOwner(std::unique_ptr<UserContext> context,
                            absl::optional<CryptohomeError> error);

  const bool is_ephemeral_mount_enforced_;
  std::unique_ptr<SafeModeDelegate> safe_mode_delegate_;
  std::unique_ptr<AuthFactorEditor> auth_factor_editor_;
  std::unique_ptr<AuthPerformer> auth_performer_;
  std::unique_ptr<MountPerformer> mount_performer_;

  base::WeakPtrFactory<AuthSessionAuthenticator> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_
