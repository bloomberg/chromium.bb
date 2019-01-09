// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/logging.h"

namespace identity {

AccountsMutatorImpl::AccountsMutatorImpl(
    ProfileOAuth2TokenService* token_service) {
  DCHECK(token_service);
}

AccountsMutatorImpl::~AccountsMutatorImpl() {}

}  // namespace identity
