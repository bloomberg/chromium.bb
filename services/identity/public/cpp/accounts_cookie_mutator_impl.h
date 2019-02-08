// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_

#include "base/macros.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"

class GaiaCookieManagerService;

namespace identity {

// Concrete implementation of the AccountsCookieMutator interface.
class AccountsCookieMutatorImpl : public AccountsCookieMutator {
 public:
  explicit AccountsCookieMutatorImpl(
      GaiaCookieManagerService* gaia_cookie_manager_service);
  ~AccountsCookieMutatorImpl() override;

 private:
  GaiaCookieManagerService* gaia_cookie_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutatorImpl);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
