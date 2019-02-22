// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_IDENTITY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_IDENTITY_PROVIDER_H_

#include "base/macros.h"
#include "components/invalidation/public/identity_provider.h"

namespace chromeos {

class DeviceOAuth2TokenService;

// Identity provider implementation backed by DeviceOAuth2TokenService.
class DeviceIdentityProvider : public invalidation::IdentityProvider,
                               public OAuth2TokenService::Observer {
 public:
  explicit DeviceIdentityProvider(
      chromeos::DeviceOAuth2TokenService* token_service);
  ~DeviceIdentityProvider() override;

  // IdentityProvider:
  std::string GetActiveAccountId() override;
  bool IsActiveAccountAvailable() override;
  std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
  FetchAccessToken(
      const std::string& oauth_consumer_name,
      const OAuth2TokenService::ScopeSet& scopes,
      invalidation::ActiveAccountAccessTokenCallback callback) override;
  void InvalidateAccessToken(const OAuth2TokenService::ScopeSet& scopes,
                             const std::string& access_token) override;
  void SetActiveAccountId(const std::string& account_id) override;

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

 private:
  chromeos::DeviceOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceIdentityProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_IDENTITY_PROVIDER_H_
