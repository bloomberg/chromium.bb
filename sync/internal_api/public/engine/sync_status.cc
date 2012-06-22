// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/engine/sync_status.h"

namespace csync {

SyncStatus::SyncStatus()
    : notifications_enabled(false),
      notifications_received(0),
      encryption_conflicts(0),
      hierarchy_conflicts(0),
      simple_conflicts(0),
      server_conflicts(0),
      committed_count(0),
      syncing(false),
      initial_sync_ended(false),
      updates_available(0),
      updates_received(0),
      reflected_updates_received(0),
      tombstone_updates_received(0),
      num_commits_total(0),
      num_local_overwrites_total(0),
      num_server_overwrites_total(0),
      nonempty_get_updates(0),
      empty_get_updates(0),
      sync_cycles_with_commits(0),
      sync_cycles_without_commits(0),
      useless_sync_cycles(0),
      useful_sync_cycles(0),
      cryptographer_ready(false),
      crypto_has_pending_keys(false) {
}

SyncStatus::~SyncStatus() {
}

}  // namespace csync
