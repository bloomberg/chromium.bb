// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_manager.h"

#include <utility>

#include "base/time/time.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace identity {

IdentityManager::AccessTokenRequest::AccessTokenRequest(
    const std::string& account_id,
    const ScopeSet& scopes,
    const std::string& consumer_id,
    GetAccessTokenCallback consumer_callback,
    ProfileOAuth2TokenService* token_service,
    IdentityManager* manager)
    : OAuth2TokenService::Consumer(consumer_id),
      token_service_(token_service),
      consumer_callback_(std::move(consumer_callback)),
      manager_(manager) {
  token_service_request_ =
      token_service_->StartRequest(account_id, scopes, this);
}

IdentityManager::AccessTokenRequest::~AccessTokenRequest() = default;

void IdentityManager::AccessTokenRequest::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  OnRequestCompleted(request, access_token, expiration_time,
                     GoogleServiceAuthError::AuthErrorNone());
}

void IdentityManager::AccessTokenRequest::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  OnRequestCompleted(request, base::nullopt, base::Time(), error);
}

void IdentityManager::AccessTokenRequest::OnRequestCompleted(
    const OAuth2TokenService::Request* request,
    const base::Optional<std::string>& access_token,
    base::Time expiration_time,
    const GoogleServiceAuthError& error) {
  std::move(consumer_callback_).Run(access_token, expiration_time, error);

  // Causes |this| to be deleted.
  manager_->AccessTokenRequestCompleted(this);
}

// static
void IdentityManager::Create(mojom::IdentityManagerRequest request,
                             SigninManagerBase* signin_manager,
                             ProfileOAuth2TokenService* token_service) {
  mojo::MakeStrongBinding(
      base::MakeUnique<IdentityManager>(signin_manager, token_service),
      std::move(request));
}

IdentityManager::IdentityManager(SigninManagerBase* signin_manager,
                                 ProfileOAuth2TokenService* token_service)
    : signin_manager_(signin_manager), token_service_(token_service) {}

IdentityManager::~IdentityManager() {}

void IdentityManager::GetPrimaryAccountInfo(
    GetPrimaryAccountInfoCallback callback) {
  std::move(callback).Run(signin_manager_->GetAuthenticatedAccountInfo());
}

void IdentityManager::GetAccessToken(const std::string& account_id,
                                     const ScopeSet& scopes,
                                     const std::string& consumer_id,
                                     GetAccessTokenCallback callback) {
  std::unique_ptr<AccessTokenRequest> access_token_request =
      base::MakeUnique<AccessTokenRequest>(account_id, scopes, consumer_id,
                                           std::move(callback), token_service_,
                                           this);

  access_token_requests_[access_token_request.get()] =
      std::move(access_token_request);
}

void IdentityManager::AccessTokenRequestCompleted(AccessTokenRequest* request) {
  access_token_requests_.erase(request);
}

}  // namespace identity
