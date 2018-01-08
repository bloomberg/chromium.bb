// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_manager.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace identity {

IdentityManager::IdentityManager(SigninManagerBase* signin_manager,
                                 ProfileOAuth2TokenService* token_service)
    : signin_manager_(signin_manager), token_service_(token_service) {
#if defined(OS_CHROMEOS)
  // On ChromeOS, the authenticated account is set very early in startup and
  // never changed. No client should be accessing the IdentityManager before the
  // authenticated account is set.
  DCHECK(signin_manager_->IsAuthenticated());
#endif
  primary_account_info_ = signin_manager_->GetAuthenticatedAccountInfo();
  signin_manager_->AddObserver(this);
}

IdentityManager::~IdentityManager() {
  signin_manager_->RemoveObserver(this);
}

AccountInfo IdentityManager::GetPrimaryAccountInfo() {
  return primary_account_info_;
}

std::unique_ptr<PrimaryAccountAccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForPrimaryAccount(
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    PrimaryAccountAccessTokenFetcher::TokenCallback callback,
    PrimaryAccountAccessTokenFetcher::Mode mode) {
  return base::MakeUnique<PrimaryAccountAccessTokenFetcher>(
      oauth_consumer_name, signin_manager_, token_service_, scopes,
      std::move(callback), mode);
}

void IdentityManager::RemoveAccessTokenFromCache(
    const AccountInfo& account_info,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  // Call PO2TS asynchronously to mimic the eventual interaction with the
  // Identity Service.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&OAuth2TokenService::InvalidateAccessToken,
                                base::Unretained(token_service_),
                                account_info.account_id, scopes, access_token));
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IdentityManager::GoogleSigninSucceeded(const AccountInfo& account_info) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&IdentityManager::HandleGoogleSigninSucceeded,
                                base::Unretained(this), account_info));
}

void IdentityManager::GoogleSignedOut(const AccountInfo& account_info) {
  // Fire observer callbacks asynchronously to mimic the asynchronous flow that
  // will be present
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&IdentityManager::HandleGoogleSignedOut,
                                base::Unretained(this), account_info));
}

void IdentityManager::HandleGoogleSigninSucceeded(
    const AccountInfo& account_info) {
  primary_account_info_ = account_info;
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountSet(account_info);
  }
}

void IdentityManager::HandleGoogleSignedOut(const AccountInfo& account_info) {
  DCHECK(account_info.account_id == primary_account_info_.account_id);
  DCHECK(account_info.gaia == primary_account_info_.gaia);
  DCHECK(account_info.email == primary_account_info_.email);
  primary_account_info_ = AccountInfo();
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountCleared(account_info);
  }
}

}  // namespace identity
