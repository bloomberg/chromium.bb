// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/all_status.h"

#include <algorithm>

#include "base/logging.h"
#include "base/port.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/sessions/session_state.h"
#include "sync/syncable/model_type.h"

namespace browser_sync {

AllStatus::AllStatus() {
  status_.initial_sync_ended = true;
  status_.notifications_enabled = false;
  status_.cryptographer_ready = false;
  status_.crypto_has_pending_keys = false;
}

AllStatus::~AllStatus() {
}

sync_api::SyncManager::Status AllStatus::CreateBlankStatus() const {
  // Status is initialized with the previous status value.  Variables
  // whose values accumulate (e.g. lifetime counters like updates_received)
  // are not to be cleared here.
  sync_api::SyncManager::Status status = status_;
  status.unsynced_count = 0;
  status.encryption_conflicts = 0;
  status.hierarchy_conflicts = 0;
  status.simple_conflicts = 0;
  status.server_conflicts = 0;
  status.committed_count = 0;
  status.initial_sync_ended = false;
  status.updates_available = 0;
  return status;
}

sync_api::SyncManager::Status AllStatus::CalcSyncing(
    const SyncEngineEvent &event) const {
  sync_api::SyncManager::Status status = CreateBlankStatus();
  const sessions::SyncSessionSnapshot& snapshot = event.snapshot;
  status.unsynced_count = static_cast<int>(snapshot.unsynced_count());
  status.encryption_conflicts = snapshot.num_encryption_conflicts();
  status.hierarchy_conflicts = snapshot.num_hierarchy_conflicts();
  status.simple_conflicts = snapshot.num_simple_conflicts();
  status.server_conflicts = snapshot.num_server_conflicts();
  status.committed_count = snapshot.syncer_status().num_successful_commits;

  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_BEGIN) {
    status.syncing = true;
  } else if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    status.syncing = false;
  }

  status.initial_sync_ended |= snapshot.is_share_usable();

  status.updates_available += snapshot.num_server_changes_remaining();
  status.sync_protocol_error = snapshot.errors().sync_protocol_error;

  // Accumulate update count only once per session to avoid double-counting.
  // TODO(ncarter): Make this realtime by having the syncer_status
  // counter preserve its value across sessions.  http://crbug.com/26339
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    status.updates_received +=
        snapshot.syncer_status().num_updates_downloaded_total;
    status.tombstone_updates_received +=
        snapshot.syncer_status().num_tombstone_updates_downloaded_total;
    status.reflected_updates_received +=
        snapshot.syncer_status().num_reflected_updates_downloaded_total;
    status.num_local_overwrites_total +=
        snapshot.syncer_status().num_local_overwrites;
    status.num_server_overwrites_total +=
        snapshot.syncer_status().num_server_overwrites;
    if (snapshot.syncer_status().num_updates_downloaded_total == 0) {
      ++status.empty_get_updates;
    } else {
      ++status.nonempty_get_updates;
    }
    if (snapshot.syncer_status().num_successful_commits == 0) {
      ++status.sync_cycles_without_commits;
    } else {
      ++status.sync_cycles_with_commits;
    }
    if (snapshot.syncer_status().num_successful_commits == 0 &&
        snapshot.syncer_status().num_updates_downloaded_total == 0) {
      ++status.useless_sync_cycles;
    } else {
      ++status.useful_sync_cycles;
    }
  }
  return status;
}

void AllStatus::OnSyncEngineEvent(const SyncEngineEvent& event) {
  ScopedStatusLock lock(this);
  switch (event.what_happened) {
    case SyncEngineEvent::SYNC_CYCLE_BEGIN:
    case SyncEngineEvent::STATUS_CHANGED:
    case SyncEngineEvent::SYNC_CYCLE_ENDED:
      status_ = CalcSyncing(event);
      break;
    case SyncEngineEvent::STOP_SYNCING_PERMANENTLY:
    case SyncEngineEvent::UPDATED_TOKEN:
    case SyncEngineEvent::CLEAR_SERVER_DATA_FAILED:
    case SyncEngineEvent::CLEAR_SERVER_DATA_SUCCEEDED:
       break;
    case SyncEngineEvent::ACTIONABLE_ERROR:
      status_ = CreateBlankStatus();
      status_.sync_protocol_error = event.snapshot.errors().sync_protocol_error;
      break;
    default:
      LOG(ERROR) << "Unrecognized Syncer Event: " << event.what_happened;
      break;
  }
}

sync_api::SyncManager::Status AllStatus::status() const {
  base::AutoLock lock(mutex_);
  return status_;
}

void AllStatus::SetNotificationsEnabled(bool notifications_enabled) {
  ScopedStatusLock lock(this);
  status_.notifications_enabled = notifications_enabled;
}

void AllStatus::IncrementNotificationsReceived() {
  ScopedStatusLock lock(this);
  ++status_.notifications_received;
}

void AllStatus::SetEncryptedTypes(syncable::ModelTypeSet types) {
  ScopedStatusLock lock(this);
  status_.encrypted_types = types;
}

void AllStatus::SetCryptographerReady(bool ready) {
  ScopedStatusLock lock(this);
  status_.cryptographer_ready = ready;
}

void AllStatus::SetCryptoHasPendingKeys(bool has_pending_keys) {
  ScopedStatusLock lock(this);
  status_.crypto_has_pending_keys = has_pending_keys;
}

void AllStatus::SetUniqueId(const std::string& guid) {
  ScopedStatusLock lock(this);
  status_.unique_id = guid;
}

ScopedStatusLock::ScopedStatusLock(AllStatus* allstatus)
    : allstatus_(allstatus) {
  allstatus->mutex_.Acquire();
}

ScopedStatusLock::~ScopedStatusLock() {
  allstatus_->mutex_.Release();
}

}  // namespace browser_sync
