// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"

#include <vector>

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
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->AddAccountToCookie(
      account_id, source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::AddAccountToCookieWithToken(
    const std::string& account_id,
    const std::string& access_token,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->AddAccountToCookieWithToken(
      account_id, access_token, source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::SetAccountsInCookie(
    const std::vector<std::string>& account_ids,
    gaia::GaiaSource source,
    base::OnceCallback<void(signin::SetAccountsInCookieResult)>
        set_accounts_in_cookies_completed_callback) {
  gaia_cookie_manager_service_->SetAccountsInCookie(
      account_ids, source,
      std::move(set_accounts_in_cookies_completed_callback));
}

void AccountsCookieMutatorImpl::TriggerCookieJarUpdate() {
  gaia_cookie_manager_service_->TriggerListAccounts();
}

void AccountsCookieMutatorImpl::LogOutAllAccounts(gaia::GaiaSource source) {
  gaia_cookie_manager_service_->LogOutAllAccounts(source);
}

}  // namespace identity
