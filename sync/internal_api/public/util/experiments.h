// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_EXPERIMENTS_
#define SYNC_UTIL_EXPERIMENTS_

#include <string>

#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

const char kFaviconSyncTag[] = "favicon_sync";
const char kPreCommitUpdateAvoidanceTag[] = "pre_commit_update_avoidance";
const char kEnhancedBookmarksTag[] = "enhanced_bookmarks";
const char kGCMInvalidationsTag[] = "gcm_invalidations";
const char kWalletSyncTag[] = "wallet_sync";

// A structure to hold the enable status of experimental sync features.
struct Experiments {
  Experiments()
      : favicon_sync_limit(200),
        enhanced_bookmarks_enabled(false),
        gcm_invalidations_enabled(true),  // By default GCM channel is enabled.
        wallet_sync_enabled(false) {}

  bool Matches(const Experiments& rhs) {
    return (favicon_sync_limit == rhs.favicon_sync_limit &&
            enhanced_bookmarks_enabled == rhs.enhanced_bookmarks_enabled &&
            enhanced_bookmarks_ext_id == rhs.enhanced_bookmarks_ext_id &&
            gcm_invalidations_enabled == rhs.gcm_invalidations_enabled &&
            wallet_sync_enabled == rhs.wallet_sync_enabled);
  }

  // The number of favicons that a client is permitted to sync.
  int favicon_sync_limit;

  // Enable the enhanced bookmarks sync datatype.
  bool enhanced_bookmarks_enabled;

  // Enable invalidations over GCM channel.
  bool gcm_invalidations_enabled;

  // Enhanced bookmarks extension id.
  std::string enhanced_bookmarks_ext_id;

  // Enable the Wallet Autofill sync datatype.
  bool wallet_sync_enabled;
};

}  // namespace syncer

#endif  // SYNC_UTIL_EXPERIMENTS_
