// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_

#include "base/macros.h"

namespace identity {

// AccountsMutator is the interface to support seeding of account info and
// mutation of refresh tokens for the user's Gaia accounts.
class AccountsMutator {
 public:
  AccountsMutator() = default;
  virtual ~AccountsMutator() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsMutator);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_MUTATOR_H_
