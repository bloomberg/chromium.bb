// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator.h"

#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace identity {

AccountsMutator::AccountsMutator(ProfileOAuth2TokenService* token_service)
    : token_service_(token_service) {}

AccountsMutator::~AccountsMutator() {}

void AccountsMutator::LoadAccountsFromDisk(
    const std::string& primary_account_id) {
  token_service_->LoadCredentials(primary_account_id);
}

}  // namespace identity
