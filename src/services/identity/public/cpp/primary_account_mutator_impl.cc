// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator_impl.h"

#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"

namespace identity {

PrimaryAccountMutatorImpl::PrimaryAccountMutatorImpl(
    AccountTrackerService* account_tracker,
    SigninManager* signin_manager)
    : account_tracker_(account_tracker), signin_manager_(signin_manager) {
  DCHECK(account_tracker_);
  DCHECK(signin_manager_);
}

PrimaryAccountMutatorImpl::~PrimaryAccountMutatorImpl() {}

bool PrimaryAccountMutatorImpl::SetPrimaryAccount(
    const std::string& account_id) {
  if (!IsSettingPrimaryAccountAllowed())
    return false;

  if (signin_manager_->IsAuthenticated())
    return false;

  AccountInfo account_info = account_tracker_->GetAccountInfo(account_id);
  if (account_info.account_id != account_id || account_info.email.empty())
    return false;

  // TODO(crbug.com/889899): should check that the account email is allowed.

  signin_manager_->OnExternalSigninCompleted(account_info.email);
  return true;
}

bool PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    ClearAccountsAction action,
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  // Check if and auth process is ongoing before reporting failure to support
  // the legacy workflow of cancelling it by clearing the primary account.
  if (!signin_manager_->IsAuthenticated() &&
      !LegacyIsPrimaryAccountAuthInProgress())
    return false;

  // TODO: report failure if SignOut is not allowed.

  switch (action) {
    case PrimaryAccountMutator::ClearAccountsAction::kDefault:
      signin_manager_->SignOut(source_metric, delete_metric);
      break;
    case PrimaryAccountMutator::ClearAccountsAction::kKeepAll:
      signin_manager_->SignOutAndKeepAllAccounts(source_metric, delete_metric);
      break;
    case PrimaryAccountMutator::ClearAccountsAction::kRemoveAll:
      signin_manager_->SignOutAndRemoveAllAccounts(source_metric,
                                                   delete_metric);
      break;
  }

  return true;
}

bool PrimaryAccountMutatorImpl::IsSettingPrimaryAccountAllowed() const {
  return signin_manager_->IsSigninAllowed();
}

void PrimaryAccountMutatorImpl::SetSettingPrimaryAccountAllowed(bool allowed) {
  signin_manager_->SetSigninAllowed(allowed);
}

bool PrimaryAccountMutatorImpl::IsClearingPrimaryAccountAllowed() const {
  NOTIMPLEMENTED();
  return false;
}

void PrimaryAccountMutatorImpl::SetClearingPrimaryAccountAllowed(bool allowed) {
  NOTIMPLEMENTED();
}

void PrimaryAccountMutatorImpl::SetAllowedPrimaryAccountPattern(
    const std::string& pattern) {
  NOTIMPLEMENTED();
}

void PrimaryAccountMutatorImpl::
    LegacyStartSigninWithRefreshTokenForPrimaryAccount(
        const std::string& refresh_token,
        const std::string& gaia_id,
        const std::string& username,
        const std::string& password,
        base::OnceCallback<void(const std::string&)> callback) {
  signin_manager_->StartSignInWithRefreshToken(refresh_token, gaia_id, username,
                                               password, std::move(callback));
}

void PrimaryAccountMutatorImpl::LegacyCompletePendingPrimaryAccountSignin() {
  signin_manager_->CompletePendingSignin();
}

void PrimaryAccountMutatorImpl::LegacyMergeSigninCredentialIntoCookieJar() {
  NOTIMPLEMENTED();
}

bool PrimaryAccountMutatorImpl::LegacyIsPrimaryAccountAuthInProgress() const {
  return signin_manager_->AuthInProgress();
}

AccountInfo PrimaryAccountMutatorImpl::LegacyPrimaryAccountForAuthInProgress()
    const {
  if (!LegacyIsPrimaryAccountAuthInProgress())
    return AccountInfo{};

  AccountInfo account_info;
  account_info.account_id = signin_manager_->GetAccountIdForAuthInProgress();
  account_info.gaia = signin_manager_->GetGaiaIdForAuthInProgress();
  account_info.email = signin_manager_->GetUsernameForAuthInProgress();

  return account_info;
}

void PrimaryAccountMutatorImpl::LegacyCopyCredentialsFrom(
    const PrimaryAccountMutator& source) {
  NOTIMPLEMENTED();
}

}  // namespace identity
