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

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsMutator);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
