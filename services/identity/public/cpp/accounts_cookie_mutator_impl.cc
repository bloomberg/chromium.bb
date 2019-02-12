// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"

#include "components/signin/core/browser/gaia_cookie_manager_service.h"

namespace identity {

AccountsCookieMutatorImpl::AccountsCookieMutatorImpl(
    GaiaCookieManagerService* gaia_cookie_manager_service)
    : gaia_cookie_manager_service_(gaia_cookie_manager_service) {
  DCHECK(gaia_cookie_manager_service_);
}

AccountsCookieMutatorImpl::~AccountsCookieMutatorImpl() {}

void AccountsCookieMutatorImpl::AddAccountToCookie(
    const std::string& account_id,
    gaia::GaiaSource source) {
  gaia_cookie_manager_service_->AddAccountToCookie(account_id, source);
}

void AccountsCookieMutatorImpl::AddAccountToCookieWithToken(
    const std::string& account_id,
    const std::string& access_token,
    gaia::GaiaSource source) {
  gaia_cookie_manager_service_->AddAccountToCookieWithToken(
      account_id, access_token, source);
}

}  // namespace identity
