// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_MANAGER_H_
#define SERVICES_IDENTITY_IDENTITY_MANAGER_H_

#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/interfaces/identity_manager.mojom.h"

class SigninManagerBase;

namespace identity {

class IdentityManager : public mojom::IdentityManager {
 public:
  static void Create(mojom::IdentityManagerRequest request,
                     SigninManagerBase* signin_manager,
                     ProfileOAuth2TokenService* token_service);

  IdentityManager(SigninManagerBase* signin_manager,
                  ProfileOAuth2TokenService* token_service);
  ~IdentityManager() override;

 private:
  // Makes an access token request to the OAuth2TokenService on behalf of a
  // given consumer that has made the request to the Identity Service.
  class AccessTokenRequest : public OAuth2TokenService::Consumer {
   public:
    AccessTokenRequest(const std::string& account_id,
                       const ScopeSet& scopes,
                       const std::string& consumer_id,
                       GetAccessTokenCallback consumer_callback,
                       ProfileOAuth2TokenService* token_service,
                       IdentityManager* manager);
    ~AccessTokenRequest() override;

   private:
    // OAuth2TokenService::Consumer:
    void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                           const std::string& access_token,
                           const base::Time& expiration_time) override;
    void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                           const GoogleServiceAuthError& error) override;

    // Completes the pending access token request by calling back the consumer.
    void OnRequestCompleted(const OAuth2TokenService::Request* request,
                            const base::Optional<std::string>& access_token,
                            base::Time expiration_time,
                            const GoogleServiceAuthError& error);

    ProfileOAuth2TokenService* token_service_;
    std::unique_ptr<OAuth2TokenService::Request> token_service_request_;
    GetAccessTokenCallback consumer_callback_;
    IdentityManager* manager_;
  };
  using AccessTokenRequests =
      std::map<AccessTokenRequest*, std::unique_ptr<AccessTokenRequest>>;

  // mojom::IdentityManager:
  void GetPrimaryAccountId(GetPrimaryAccountIdCallback callback) override;
  void GetAccessToken(const std::string& account_id,
                      const ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override;

  // Deletes |request|.
  void AccessTokenRequestCompleted(AccessTokenRequest* request);

  SigninManagerBase* signin_manager_;
  ProfileOAuth2TokenService* token_service_;

  // The set of pending requests for access tokens.
  AccessTokenRequests access_token_requests_;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_MANAGER_H_
