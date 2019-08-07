// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

class SigninClient;

namespace signin {

// Fetches multilogin access tokens in parallel for multiple accounts.
// It is safe to delete this object from within the callbacks.
class OAuthMultiloginTokenFetcher : public OAuth2TokenService::Consumer {
 public:
  using SuccessCallback = base::OnceCallback<void(
      const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>&)>;
  using FailureCallback =
      base::OnceCallback<void(const GoogleServiceAuthError&)>;

  OAuthMultiloginTokenFetcher(SigninClient* signin_client,
                              OAuth2TokenService* token_service,
                              const std::vector<std::string>& account_ids,
                              SuccessCallback success_callback,
                              FailureCallback failure_callback);

  ~OAuthMultiloginTokenFetcher() override;

 private:
  void StartFetchingToken(const std::string& account_id);

  // Overridden from OAuth2TokenService::Consumer.
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Helper function to remove a request from token_requests_.
  void EraseRequest(const OAuth2TokenService::Request* request);

  SigninClient* signin_client_;
  OAuth2TokenService* token_service_;
  const std::vector<std::string> account_ids_;

  SuccessCallback success_callback_;
  FailureCallback failure_callback_;

  std::vector<std::unique_ptr<OAuth2TokenService::Request>> token_requests_;
  std::map<std::string, std::string> access_tokens_;
  std::set<std::string> retried_requests_;  // Requests are retried once.

  base::WeakPtrFactory<OAuthMultiloginTokenFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OAuthMultiloginTokenFetcher);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_
