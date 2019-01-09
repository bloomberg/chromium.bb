// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_

#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/signin_metrics.h"

namespace identity {

// AccountsMutator is the interface to support seeding of account info and
// mutation of refresh tokens for the user's Gaia accounts.
class AccountsMutator {
 public:
  AccountsMutator() = default;
  virtual ~AccountsMutator() = default;

  // Removes the account given by |account_id|. Also revokes the token
  // server-side if needed.
  virtual void RemoveAccount(
      const std::string& account_id,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown) = 0;

  // Removes all accounts.
  virtual void RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsMutator);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
