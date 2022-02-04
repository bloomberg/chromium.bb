// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_AUTHENTICATION_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_AUTHENTICATION_ASH_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {
class ExtendedAuthenticator;
}  // namespace ash

namespace extensions {
namespace api {
namespace quick_unlock_private {
struct TokenInfo;
}  // namespace quick_unlock_private
}  // namespace api
}  // namespace extensions

namespace crosapi {

// This class is the ash-chrome implementation of the Authentication
// interface. This class must only be used from the main thread.
class AuthenticationAsh : public mojom::Authentication {
 public:
  using TokenInfo = extensions::api::quick_unlock_private::TokenInfo;

  AuthenticationAsh();
  AuthenticationAsh(const AuthenticationAsh&) = delete;
  AuthenticationAsh& operator=(const AuthenticationAsh&) = delete;
  ~AuthenticationAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Authentication> receiver);

  // crosapi::mojom::Authentication:
  void CreateQuickUnlockPrivateTokenInfo(
      const std::string& password,
      CreateQuickUnlockPrivateTokenInfoCallback callback) override;
  void IsOsReauthAllowedForActiveUserProfile(
      base::TimeDelta auth_token_lifetime,
      IsOsReauthAllowedForActiveUserProfileCallback callback) override;

 private:
  // Continuation of CreateQuickUnlockPrivateTokenInfo(). Last 3 params match
  // extensions::QuickUnlockPrivateGetAuthTokenHelper::ResultCallback.
  void OnCreateQuickUnlockPrivateTokenInfoResults(
      CreateQuickUnlockPrivateTokenInfoCallback callback,
      scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator,
      bool success,
      std::unique_ptr<TokenInfo> token_info,
      const std::string& error_message);

  mojo::ReceiverSet<mojom::Authentication> receivers_;

  base::WeakPtrFactory<AuthenticationAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_AUTHENTICATION_ASH_H_
