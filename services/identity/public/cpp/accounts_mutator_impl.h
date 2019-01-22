// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_

#include "base/macros.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/accounts_mutator.h"

class AccountTrackerService;
class ProfileOAuth2TokenService;
class SigninManagerBase;

namespace identity {

// Concrete implementation of the AccountsMutatorImpl interface.
class AccountsMutatorImpl : public AccountsMutator {
 public:
  explicit AccountsMutatorImpl(ProfileOAuth2TokenService* token_service,
                               AccountTrackerService* account_tracker_service,
                               SigninManagerBase* signin_manager);
  ~AccountsMutatorImpl() override;

  // Updates the information of the account associated with |gaia_id|, first
  // adding that account to the system if it is not known.
  std::string AddOrUpdateAccount(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& refresh_token,
      bool is_under_advanced_protection,
      signin_metrics::SourceForRefreshTokenOperation source) override;

  // Updates the information about account identified by |account_id|.
  void UpdateAccountInfo(
      const std::string& account_id,
      base::Optional<bool> is_child_account,
      base::Optional<bool> is_under_advanced_protection) override;

  // Removes the account given by |account_id|. Also revokes the token
  // server-side if needed.
  void RemoveAccount(
      const std::string& account_id,
      signin_metrics::SourceForRefreshTokenOperation source) override;

  // Removes all accounts.
  void RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation source) override;

  // Invalidates the refresh token of the primary account.
  // The primary account must necessarily be set by the time this method
  // is invoked.
  void InvalidateRefreshTokenForPrimaryAccount(
      signin_metrics::SourceForRefreshTokenOperation source) override;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Removes the credentials associated to account_id from the internal storage,
  // and moves them to |target|. The credentials are not revoked on the server,
  // but the IdentityManager::Observer::OnRefreshTokenRemovedForAccount()
  // notification is sent to the observers.
  void MoveAccount(AccountsMutator* target,
                   const std::string& account_id) override;
#endif

 private:
  ProfileOAuth2TokenService* token_service_;
  AccountTrackerService* account_tracker_service_;
  SigninManagerBase* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorImpl);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_
