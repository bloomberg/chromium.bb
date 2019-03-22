// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_

#include "components/signin/core/browser/account_info.h"

namespace identity {
class IdentityManager;
}  // namespace identity

namespace syncer {

struct SyncAccountInfo {
  SyncAccountInfo();
  SyncAccountInfo(const AccountInfo& account_info, bool is_primary);

  AccountInfo account_info;
  bool is_primary = false;
};

// Determines which account should be used for Sync and returns the
// corresponding SyncAccountInfo. This is exposed so that autofill metrics
// code can use it.
SyncAccountInfo DetermineAccountToUse(
    identity::IdentityManager* identity_manager,
    bool allow_secondary_accounts);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_
