// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_manager.h"

#include <utility>

#include "base/time/time.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/interfaces/account.mojom.h"

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
                             AccountTrackerService* account_tracker,
                             SigninManagerBase* signin_manager,
                             ProfileOAuth2TokenService* token_service) {
  new IdentityManager(std::move(request), account_tracker, signin_manager,
                      token_service);
}

IdentityManager::IdentityManager(mojom::IdentityManagerRequest request,
                                 AccountTrackerService* account_tracker,
                                 SigninManagerBase* signin_manager,
                                 ProfileOAuth2TokenService* token_service)
    : binding_(this, std::move(request)),
      account_tracker_(account_tracker),
      signin_manager_(signin_manager),
      token_service_(token_service) {
  signin_manager_shutdown_subscription_ =
      signin_manager_->RegisterOnShutdownCallback(base::Bind(
          &IdentityManager::OnSigninManagerShutdown, base::Unretained(this)));
  binding_.set_connection_error_handler(
      base::Bind(&IdentityManager::OnConnectionError, base::Unretained(this)));

  token_service_->AddObserver(this);
  signin_manager_->AddObserver(this);
}

IdentityManager::~IdentityManager() {
  token_service_->RemoveObserver(this);
  signin_manager_->RemoveObserver(this);
  binding_.Close();
}

void IdentityManager::GetPrimaryAccountInfo(
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

void IdentityManager::GetPrimaryAccountWhenAvailable(
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

void IdentityManager::GetAccountInfoFromGaiaId(
    const std::string& gaia_id,
    GetAccountInfoFromGaiaIdCallback callback) {
  AccountInfo account_info = account_tracker_->FindAccountInfoByGaiaId(gaia_id);
  AccountState account_state = GetStateOfAccount(account_info);
  std::move(callback).Run(account_info, account_state);
}

void IdentityManager::GetAccounts(GetAccountsCallback callback) {
  std::vector<mojom::AccountPtr> accounts;

  for (const std::string& account_id : token_service_->GetAccounts()) {
    AccountInfo account_info = account_tracker_->GetAccountInfo(account_id);
    AccountState account_state = GetStateOfAccount(account_info);

    mojom::AccountPtr account =
        mojom::Account::New(account_info, account_state);

    if (account->state.is_primary_account) {
      accounts.insert(accounts.begin(), std::move(account));
    } else {
      accounts.push_back(std::move(account));
    }
  }

  std::move(callback).Run(std::move(accounts));
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

void IdentityManager::OnRefreshTokenAvailable(const std::string& account_id) {
  OnAccountStateChange(account_id);
}

void IdentityManager::GoogleSigninSucceeded(const std::string& account_id,
                                            const std::string& username) {
  OnAccountStateChange(account_id);
}

void IdentityManager::OnAccountStateChange(const std::string& account_id) {
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

void IdentityManager::AccessTokenRequestCompleted(AccessTokenRequest* request) {
  access_token_requests_.erase(request);
}

AccountState IdentityManager::GetStateOfAccount(
    const AccountInfo& account_info) {
  AccountState account_state;
  account_state.has_refresh_token =
      token_service_->RefreshTokenIsAvailable(account_info.account_id);
  account_state.is_primary_account =
      (account_info.account_id == signin_manager_->GetAuthenticatedAccountId());
  return account_state;
}

void IdentityManager::OnSigninManagerShutdown() {
  delete this;
}

void IdentityManager::OnConnectionError() {
  delete this;
}

}  // namespace identity
