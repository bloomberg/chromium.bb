// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_EXPERIMENTS_
#define SYNC_UTIL_EXPERIMENTS_

namespace syncer {

const char kFaviconSyncTag[] = "favicon_sync";
const char kPreCommitUpdateAvoidanceTag[] = "pre_commit_update_avoidance";
const char kGCMChannelTag[] = "gcm_channel";

// A structure to hold the enable status of experimental sync features.
struct Experiments {
  enum GCMChannelState {
    UNSET,
    SUPPRESSED,
    ENABLED,
  };

  Experiments() : favicon_sync_limit(200),
                  gcm_channel_state(UNSET) {}

  bool Matches(const Experiments& rhs) {
    return (favicon_sync_limit == rhs.favicon_sync_limit &&
            gcm_channel_state == rhs.gcm_channel_state);
  }

  // The number of favicons that a client is permitted to sync.
  int favicon_sync_limit;

  // Enable state of the GCM channel.
  GCMChannelState gcm_channel_state;
};

}  // namespace syncer

#endif  // SYNC_UTIL_EXPERIMENTS_
