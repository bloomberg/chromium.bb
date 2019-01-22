// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"

namespace identity {

AccountsMutatorImpl::AccountsMutatorImpl(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    SigninManagerBase* signin_manager)
    : token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      signin_manager_(signin_manager) {
  DCHECK(token_service_);
  DCHECK(account_tracker_service_);
  DCHECK(signin_manager_);
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

void AccountsMutatorImpl::UpdateAccountInfo(
    const std::string& account_id,
    base::Optional<bool> is_child_account,
    base::Optional<bool> is_under_advanced_protection) {
  if (is_child_account.has_value()) {
    account_tracker_service_->SetIsChildAccount(account_id,
                                                is_child_account.value());
  }

  if (is_under_advanced_protection.has_value()) {
    account_tracker_service_->SetIsAdvancedProtectionAccount(
        account_id, is_under_advanced_protection.value());
  }
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

void AccountsMutatorImpl::InvalidateRefreshTokenForPrimaryAccount(
    signin_metrics::SourceForRefreshTokenOperation source) {
  DCHECK(signin_manager_->IsAuthenticated());
  AccountInfo primary_account_info =
      signin_manager_->GetAuthenticatedAccountInfo();
  AddOrUpdateAccount(primary_account_info.gaia, primary_account_info.email,
                     OAuth2TokenServiceDelegate::kInvalidRefreshToken,
                     primary_account_info.is_under_advanced_protection, source);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AccountsMutatorImpl::MoveAccount(AccountsMutator* target,
                                      const std::string& account_id) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.account_id.empty());

  auto* target_impl = static_cast<AccountsMutatorImpl*>(target);
  target_impl->account_tracker_service_->SeedAccountInfo(account_info);
  token_service_->ExtractCredentials(target_impl->token_service_, account_id);
}
#endif

}  // namespace identity
