// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace identity {

AccountsMutatorImpl::AccountsMutatorImpl(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service)
    : token_service_(token_service),
      account_tracker_service_(account_tracker_service) {
  DCHECK(token_service_);
  DCHECK(account_tracker_service_);
}

AccountsMutatorImpl::~AccountsMutatorImpl() {}

std::string AccountsMutatorImpl::AddOrUpdateAccount(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& refresh_token,
    bool is_under_advanced_protection,
    signin_metrics::SourceForRefreshTokenOperation source) {
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(gaia_id, email);
  account_tracker_service_->SetIsAdvancedProtectionAccount(
      account_id, is_under_advanced_protection);
  token_service_->UpdateCredentials(account_id, refresh_token, source);

  return account_id;
}

void AccountsMutatorImpl::RemoveAccount(
    const std::string& account_id,
    signin_metrics::SourceForRefreshTokenOperation source) {
  token_service_->RevokeCredentials(account_id, source);
}

void AccountsMutatorImpl::RemoveAllAccounts(
    signin_metrics::SourceForRefreshTokenOperation source) {
  token_service_->RevokeAllCredentials(source);
}

}  // namespace identity
