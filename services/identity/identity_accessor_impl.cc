// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_accessor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/mojom/account.mojom.h"

namespace identity {

IdentityAccessorImpl::AccessTokenRequest::AccessTokenRequest(
    const std::string& account_id,
    const ScopeSet& scopes,
    const std::string& consumer_id,
    GetAccessTokenCallback consumer_callback,
    ProfileOAuth2TokenService* token_service,
    IdentityAccessorImpl* manager)
    : OAuth2TokenService::Consumer(consumer_id),
      token_service_(token_service),
      consumer_callback_(std::move(consumer_callback)),
      manager_(manager) {
  token_service_request_ =
      token_service_->StartRequest(account_id, scopes, this);
}

IdentityAccessorImpl::AccessTokenRequest::~AccessTokenRequest() = default;

void IdentityAccessorImpl::AccessTokenRequest::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  OnRequestCompleted(request, token_response.access_token,
                     token_response.expiration_time,
                     GoogleServiceAuthError::AuthErrorNone());
}

void IdentityAccessorImpl::AccessTokenRequest::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  OnRequestCompleted(request, base::nullopt, base::Time(), error);
}

void IdentityAccessorImpl::AccessTokenRequest::OnRequestCompleted(
    const OAuth2TokenService::Request* request,
    const base::Optional<std::string>& access_token,
    base::Time expiration_time,
    const GoogleServiceAuthError& error) {
  std::move(consumer_callback_).Run(access_token, expiration_time, error);

  // Causes |this| to be deleted.
  manager_->AccessTokenRequestCompleted(this);
}

// static
void IdentityAccessorImpl::Create(mojom::IdentityAccessorRequest request,
                                  IdentityManager* identity_manager,
                                  AccountTrackerService* account_tracker,
                                  SigninManagerBase* signin_manager,
                                  ProfileOAuth2TokenService* token_service) {
  new IdentityAccessorImpl(std::move(request), identity_manager,
                           account_tracker, signin_manager, token_service);
}

IdentityAccessorImpl::IdentityAccessorImpl(
    mojom::IdentityAccessorRequest request,
    IdentityManager* identity_manager,
    AccountTrackerService* account_tracker,
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service)
    : binding_(this, std::move(request)),
      identity_manager_(identity_manager),
      account_tracker_(account_tracker),
      signin_manager_(signin_manager),
      token_service_(token_service) {
  binding_.set_connection_error_handler(base::BindRepeating(
      &IdentityAccessorImpl::OnConnectionError, base::Unretained(this)));

  identity_manager_->AddObserver(this);
}

IdentityAccessorImpl::~IdentityAccessorImpl() {
  identity_manager_->RemoveObserver(this);
  binding_.Close();
}

void IdentityAccessorImpl::GetPrimaryAccountInfo(
    GetPrimaryAccountInfoCallback callback) {
  // It's annoying that this can't be trivially implemented in terms of
  // GetAccountInfoFromGaiaId(), but there's no SigninManagerBase method that
  // directly returns the authenticated GAIA ID. We can of course get it from
  // the AccountInfo but once we have the AccountInfo we ... have the
  // AccountInfo.
  AccountInfo account_info = signin_manager_->GetAuthenticatedAccountInfo();
  AccountState account_state = GetStateOfAccount(account_info);
  std::move(callback).Run(account_info, account_state);
}

void IdentityAccessorImpl::GetPrimaryAccountWhenAvailable(
    GetPrimaryAccountWhenAvailableCallback callback) {
  AccountInfo account_info = signin_manager_->GetAuthenticatedAccountInfo();
  AccountState account_state = GetStateOfAccount(account_info);

  if (!account_state.has_refresh_token ||
      token_service_->RefreshTokenHasError(account_info.account_id)) {
    primary_account_available_callbacks_.push_back(std::move(callback));
    return;
  }

  DCHECK(!account_info.account_id.empty());
  DCHECK(!account_info.email.empty());
  DCHECK(!account_info.gaia.empty());
  std::move(callback).Run(account_info, account_state);
}

void IdentityAccessorImpl::GetAccountInfoFromGaiaId(
    const std::string& gaia_id,
    GetAccountInfoFromGaiaIdCallback callback) {
  AccountInfo account_info = account_tracker_->FindAccountInfoByGaiaId(gaia_id);
  AccountState account_state = GetStateOfAccount(account_info);
  std::move(callback).Run(account_info, account_state);
}

void IdentityAccessorImpl::GetAccessToken(const std::string& account_id,
                                          const ScopeSet& scopes,
                                          const std::string& consumer_id,
                                          GetAccessTokenCallback callback) {
  std::unique_ptr<AccessTokenRequest> access_token_request =
      std::make_unique<AccessTokenRequest>(account_id, scopes, consumer_id,
                                           std::move(callback), token_service_,
                                           this);

  access_token_requests_[access_token_request.get()] =
      std::move(access_token_request);
}

void IdentityAccessorImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  OnAccountStateChange(account_info.account_id);
}

void IdentityAccessorImpl::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  OnAccountStateChange(primary_account_info.account_id);
}

void IdentityAccessorImpl::OnAccountStateChange(const std::string& account_id) {
  AccountInfo account_info = account_tracker_->GetAccountInfo(account_id);
  AccountState account_state = GetStateOfAccount(account_info);

  // Check whether the primary account is available and notify any waiting
  // consumers if so.
  if (account_state.is_primary_account && account_state.has_refresh_token &&
      !token_service_->RefreshTokenHasError(account_info.account_id)) {
    DCHECK(!account_info.account_id.empty());
    DCHECK(!account_info.email.empty());
    DCHECK(!account_info.gaia.empty());

    for (auto&& callback : primary_account_available_callbacks_) {
      std::move(callback).Run(account_info, account_state);
    }
    primary_account_available_callbacks_.clear();
  }
}

void IdentityAccessorImpl::AccessTokenRequestCompleted(
    AccessTokenRequest* request) {
  access_token_requests_.erase(request);
}

AccountState IdentityAccessorImpl::GetStateOfAccount(
    const AccountInfo& account_info) {
  AccountState account_state;
  account_state.has_refresh_token =
      token_service_->RefreshTokenIsAvailable(account_info.account_id);
  account_state.is_primary_account =
      (account_info.account_id == signin_manager_->GetAuthenticatedAccountId());
  return account_state;
}

void IdentityAccessorImpl::OnConnectionError() {
  delete this;
}

}  // namespace identity
