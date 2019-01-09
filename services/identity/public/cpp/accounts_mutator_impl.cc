// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace identity {

AccountsMutatorImpl::AccountsMutatorImpl(
    ProfileOAuth2TokenService* token_service)
    : token_service_(token_service) {
  DCHECK(token_service_);
}

AccountsMutatorImpl::~AccountsMutatorImpl() {}

void AccountsMutatorImpl::RemoveAccount(
    const std::string& account_id,
    signin_metrics::SourceForRefreshTokenOperation source) {
  token_service_->RevokeCredentials(account_id, source);
}

void AccountsMutatorImpl::RemoveAllAccounts(
    signin_metrics::SourceForRefreshTokenOperation source) {
  token_service_->RevokeAllCredentials(source);
}

}  // namespace identity
