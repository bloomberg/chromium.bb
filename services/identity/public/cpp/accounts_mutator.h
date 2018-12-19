// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_

#include <string>

#include "base/macros.h"

class ProfileOAuth2TokenService;

namespace identity {

// Supports seeding of account info and mutation of refresh tokens for the
// user's Gaia accounts.
class AccountsMutator {
 public:
  explicit AccountsMutator(ProfileOAuth2TokenService* token_service);
  ~AccountsMutator();

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  //
  // The primary account is specified with the |primary_account_id| argument.
  // For a regular profile, the primary account id comes from IdentityManager.
  // For a supervised user, the id comes from SupervisedUserService.
  // NOTE: In normal usage this method SHOULD NOT be called as the loading of
  // accounts from disk occurs as part of the internal startup flow. The method
  // is only used in production for a very small number of corner case startup
  // flows.
  // TODO(https://crbug.com/740117): Eliminate the need to expose this.
  void LoadAccountsFromDisk(const std::string& primary_account_id);

 private:
  ProfileOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(AccountsMutator);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
