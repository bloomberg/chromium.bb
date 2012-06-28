// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/model_neutral_state.h"

namespace syncer {
namespace sessions {

ModelNeutralState::ModelNeutralState()
    : num_successful_commits(0),
      num_successful_bookmark_commits(0),
      num_updates_downloaded_total(0),
      num_tombstone_updates_downloaded_total(0),
      num_reflected_updates_downloaded_total(0),
      num_local_overwrites(0),
      num_server_overwrites(0),
      last_download_updates_result(UNSET),
      commit_result(UNSET),
      conflicts_resolved(false),
      items_committed(false),
      debug_info_sent(false),
      num_server_changes_remaining(0) {
}

ModelNeutralState::~ModelNeutralState() {}

}  // namespace sessions
}  // namespace syncer
