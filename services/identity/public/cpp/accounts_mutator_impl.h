// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_

#include <string>

#include "base/macros.h"
#include "build/buildflag.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/accounts_mutator.h"

class AccountTrackerService;
class PrefService;
class ProfileOAuth2TokenService;
class SigninManagerBase;

namespace identity {

// Concrete implementation of the AccountsMutatorImpl interface.
class AccountsMutatorImpl : public AccountsMutator {
 public:
  explicit AccountsMutatorImpl(ProfileOAuth2TokenService* token_service,
                               AccountTrackerService* account_tracker_service,
                               SigninManagerBase* signin_manager,
                               PrefService* pref_service);
  ~AccountsMutatorImpl() override;

  // AccountsMutator:
  std::string AddOrUpdateAccount(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& refresh_token,
      bool is_under_advanced_protection,
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void UpdateAccountInfo(
      const std::string& account_id,
      base::Optional<bool> is_child_account,
      base::Optional<bool> is_under_advanced_protection) override;
  void RemoveAccount(
      const std::string& account_id,
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void InvalidateRefreshTokenForPrimaryAccount(
      signin_metrics::SourceForRefreshTokenOperation source) override;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void MoveAccount(AccountsMutator* target,
                   const std::string& account_id) override;
#endif

  void LegacySetRefreshTokenForSupervisedUser(
      const std::string& refresh_token) override;

 private:
  ProfileOAuth2TokenService* token_service_;
  AccountTrackerService* account_tracker_service_;
  SigninManagerBase* signin_manager_;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  PrefService* pref_service_;
#endif
  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorImpl);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_
