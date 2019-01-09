// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_

#include "base/macros.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/accounts_mutator.h"

class ProfileOAuth2TokenService;

namespace identity {

// Concrete implementation of the AccountsMutatorImpl interface.
class AccountsMutatorImpl : public AccountsMutator {
 public:
  explicit AccountsMutatorImpl(ProfileOAuth2TokenService* token_service);
  ~AccountsMutatorImpl() override;

  // Removes the account given by |account_id|. Also revokes the token
  // server-side if needed.
  void RemoveAccount(
      const std::string& account_id,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown) override;

  // Removes all accounts.
  void RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown) override;

 private:
  ProfileOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorImpl);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_IMPL_H_
