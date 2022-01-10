// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_FAKE_EXTENDED_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_FAKE_EXTENDED_AUTHENTICATOR_H_

#include "base/component_export.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

class AuthFailure;

class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) FakeExtendedAuthenticator
    : public ExtendedAuthenticator {
 public:
  FakeExtendedAuthenticator(AuthStatusConsumer* consumer,
                            const UserContext& expected_user_context);

  FakeExtendedAuthenticator(const FakeExtendedAuthenticator&) = delete;
  FakeExtendedAuthenticator& operator=(const FakeExtendedAuthenticator&) =
      delete;

  // ExtendedAuthenticator:
  void SetConsumer(AuthStatusConsumer* consumer) override;
  void AuthenticateToCheck(const UserContext& context,
                           base::OnceClosure success_callback) override;
  void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EndFingerprintAuthSession() override;
  void AuthenticateWithFingerprint(
      const UserContext& context,
      base::OnceCallback<void(::user_data_auth::CryptohomeErrorCode)> callback)
      override;
  void AddKey(const UserContext& context,
              const cryptohome::KeyDefinition& key,
              bool replace_existing,
              base::OnceClosure success_callback) override;
  void RemoveKey(const UserContext& context,
                 const std::string& key_to_remove,
                 base::OnceClosure success_callback) override;
  void TransformKeyIfNeeded(const UserContext& user_context,
                            ContextCallback callback) override;

 private:
  ~FakeExtendedAuthenticator() override;

  void OnAuthSuccess(const UserContext& context);
  void OnAuthFailure(AuthState state, const AuthFailure& error);

  AuthStatusConsumer* consumer_;

  UserContext expected_user_context_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::FakeExtendedAuthenticator;
}

#endif  // CHROMEOS_LOGIN_AUTH_FAKE_EXTENDED_AUTHENTICATOR_H_
