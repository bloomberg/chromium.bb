// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_manager.h"

#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace identity {

// static
void IdentityManager::Create(mojom::IdentityManagerRequest request,
                             SigninManagerBase* signin_manager) {
  mojo::MakeStrongBinding(base::MakeUnique<IdentityManager>(signin_manager),
                          std::move(request));
}

IdentityManager::IdentityManager(SigninManagerBase* signin_manager)
    : signin_manager_(signin_manager) {}

IdentityManager::~IdentityManager() {}

void IdentityManager::GetPrimaryAccountId(
    const GetPrimaryAccountIdCallback& callback) {
  AccountId account_id = EmptyAccountId();

  if (signin_manager_->IsAuthenticated()) {
    AccountInfo account_info = signin_manager_->GetAuthenticatedAccountInfo();
    account_id =
        AccountId::FromUserEmailGaiaId(account_info.email, account_info.gaia);
  }

  callback.Run(account_id);
}

}  // namespace identity
