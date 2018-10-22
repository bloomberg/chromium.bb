// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/active_account_access_token_fetcher_impl.h"

namespace invalidation {

ActiveAccountAccessTokenFetcherImpl::ActiveAccountAccessTokenFetcherImpl(
    const std::string& active_account_id,
    const std::string& oauth_consumer_name,
    OAuth2TokenService* token_service,
    const OAuth2TokenService::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback)
    : OAuth2TokenService::Consumer(oauth_consumer_name),
      callback_(std::move(callback)) {
  access_token_request_ =
      token_service->StartRequest(active_account_id, scopes, this);
}

ActiveAccountAccessTokenFetcherImpl::~ActiveAccountAccessTokenFetcherImpl() {}

void ActiveAccountAccessTokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  HandleTokenRequestCompletion(request, GoogleServiceAuthError::AuthErrorNone(),
                               token_response.access_token);
}

void ActiveAccountAccessTokenFetcherImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  HandleTokenRequestCompletion(request, error, std::string());
}

void ActiveAccountAccessTokenFetcherImpl::HandleTokenRequestCompletion(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2TokenService::Request> request_deleter(
      std::move(access_token_request_));

  std::move(callback_).Run(error, access_token);
}

}  // namespace invalidation
