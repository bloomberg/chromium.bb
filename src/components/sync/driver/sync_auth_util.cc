// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_auth_util.h"

#include <vector>

#include "services/identity/public/cpp/identity_manager.h"

namespace syncer {

SyncAccountInfo::SyncAccountInfo() = default;

SyncAccountInfo::SyncAccountInfo(const AccountInfo& account_info,
                                 bool is_primary)
    : account_info(account_info), is_primary(is_primary) {}

SyncAccountInfo DetermineAccountToUse(
    identity::IdentityManager* identity_manager,
    bool allow_secondary_accounts) {
  // If there is a "primary account", i.e. the user explicitly chose to
  // sign-in to Chrome, then always use that account.
  if (identity_manager->HasPrimaryAccount()) {
    return SyncAccountInfo(identity_manager->GetPrimaryAccountInfo(),
                           /*is_primary=*/true);
  }

  // Otherwise, fall back to the default content area signed-in account.
  if (allow_secondary_accounts) {
    // Check if there is a content area signed-in account, and we have a refresh
    // token for it.
    std::vector<AccountInfo> cookie_accounts =
        identity_manager->GetAccountsInCookieJar();
    if (!cookie_accounts.empty() &&
        identity_manager->HasAccountWithRefreshToken(
            cookie_accounts[0].account_id)) {
      return SyncAccountInfo(cookie_accounts[0], /*is_primary=*/false);
    }
  }
  return SyncAccountInfo();
}

}  // namespace syncer
