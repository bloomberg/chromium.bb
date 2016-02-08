// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_UTIL_EXPERIMENTS_H_
#define SYNC_INTERNAL_API_PUBLIC_UTIL_EXPERIMENTS_H_

#include <string>

#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

const char kFaviconSyncTag[] = "favicon_sync";
const char kPreCommitUpdateAvoidanceTag[] = "pre_commit_update_avoidance";
const char kGCMInvalidationsTag[] = "gcm_invalidations";
const char kWalletSyncTag[] = "wallet_sync";

// A structure to hold the enable status of experimental sync features.
struct Experiments {
  Experiments()
      : favicon_sync_limit(200),
        gcm_invalidations_enabled(true),  // By default GCM channel is enabled.
        wallet_sync_enabled(false) {}

  bool Matches(const Experiments& rhs) {
    return (favicon_sync_limit == rhs.favicon_sync_limit &&
            gcm_invalidations_enabled == rhs.gcm_invalidations_enabled &&
            wallet_sync_enabled == rhs.wallet_sync_enabled);
  }

  // The number of favicons that a client is permitted to sync.
  int favicon_sync_limit;

  // Enable invalidations over GCM channel.
  bool gcm_invalidations_enabled;

  // Enable the Wallet Autofill sync datatype.
  bool wallet_sync_enabled;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_UTIL_EXPERIMENTS_H_
