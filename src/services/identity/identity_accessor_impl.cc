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
    IdentityAccessorImpl* manager)
    : consumer_callback_(std::move(consumer_callback)), manager_(manager) {
  access_token_fetcher_ =
      manager->identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id, consumer_id, scopes,
          base::BindOnce(&AccessTokenRequest::OnTokenRequestCompleted,
                         base::Unretained(this)),
          identity::AccessTokenFetcher::Mode::kImmediate);
}

IdentityAccessorImpl::AccessTokenRequest::~AccessTokenRequest() = default;

void IdentityAccessorImpl::AccessTokenRequest::OnTokenRequestCompleted(
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(consumer_callback_)
        .Run(access_token_info.token, access_token_info.expiration_time, error);
  } else {
    std::move(consumer_callback_).Run(base::nullopt, base::Time(), error);
  }

  // Causes |this| to be deleted.
  manager_->AccessTokenRequestCompleted(this);
}

// static
void IdentityAccessorImpl::Create(mojom::IdentityAccessorRequest request,
                                  IdentityManager* identity_manager,
                                  AccountTrackerService* account_tracker) {
  new IdentityAccessorImpl(std::move(request), identity_manager,
                           account_tracker);
}

IdentityAccessorImpl::IdentityAccessorImpl(
    mojom::IdentityAccessorRequest request,
    IdentityManager* identity_manager,
    AccountTrackerService* account_tracker)
    : binding_(this, std::move(request)),
      identity_manager_(identity_manager),
      account_tracker_(account_tracker) {
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
  AccountInfo account_info = identity_manager_->GetPrimaryAccountInfo();
  AccountState account_state = GetStateOfAccount(account_info);
  std::move(callback).Run(account_info, account_state);
}

void IdentityAccessorImpl::GetPrimaryAccountWhenAvailable(
    GetPrimaryAccountWhenAvailableCallback callback) {
  AccountInfo account_info = identity_manager_->GetPrimaryAccountInfo();
  AccountState account_state = GetStateOfAccount(account_info);

  if (!account_state.has_refresh_token ||
      identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id) != GoogleServiceAuthError::AuthErrorNone()) {
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
                                           std::move(callback), this);

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
      identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id) == GoogleServiceAuthError::AuthErrorNone()) {
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
      identity_manager_->HasAccountWithRefreshToken(account_info.account_id);
  account_state.is_primary_account =
      (account_info.account_id == identity_manager_->GetPrimaryAccountId());
  return account_state;
}

void IdentityAccessorImpl::OnConnectionError() {
  delete this;
}

}  // namespace identity
