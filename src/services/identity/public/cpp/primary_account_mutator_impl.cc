// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator_impl.h"

#include <utility>

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

  signin_manager_->SignIn(account_info.email);
  return true;
}

bool PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    ClearAccountsAction action,
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  if (!signin_manager_->IsAuthenticated())
    return false;

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

void PrimaryAccountMutatorImpl::SetAllowedPrimaryAccountPattern(
    const std::string& pattern) {
  NOTIMPLEMENTED();
}

}  // namespace identity
