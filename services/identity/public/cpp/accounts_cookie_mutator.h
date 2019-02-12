// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_H_

#include <string>

#include "base/macros.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace identity {

// AccountsCookieMutator is the interface to support merging known local Google
// accounts into the cookie jar tracking the list of logged-in Google sessions.
class AccountsCookieMutator {
 public:
  AccountsCookieMutator() = default;
  virtual ~AccountsCookieMutator() = default;

  // Adds an account identified by |account_id| to the cookie responsible for
  // tracking the list of logged-in Google sessions across the web.
  virtual void AddAccountToCookie(const std::string& account_id,
                                  gaia::GaiaSource source) = 0;

  // Adds an account identified by |account_id| and with |access_token| to the
  // cookie responsible for tracking the list of logged-in Google sessions
  // across the web.
  virtual void AddAccountToCookieWithToken(const std::string& account_id,
                                           const std::string& access_token,
                                           gaia::GaiaSource source) = 0;

  // Triggers a ListAccounts fetch. Can be used in circumstances where clients
  // know that the contents of the Gaia cookie might have changed.
  virtual void TriggerCookieJarUpdate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutator);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCOUNTS_COOKIE_MUTATOR_H_
