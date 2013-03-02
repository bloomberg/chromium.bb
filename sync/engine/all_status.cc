// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/all_status.h"

#include <algorithm>

#include "base/logging.h"
#include "base/port.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

AllStatus::AllStatus() {
  status_.notifications_enabled = false;
  status_.cryptographer_ready = false;
  status_.crypto_has_pending_keys = false;
}

AllStatus::~AllStatus() {
}

SyncStatus AllStatus::CreateBlankStatus() const {
  // Status is initialized with the previous status value.  Variables
  // whose values accumulate (e.g. lifetime counters like updates_received)
  // are not to be cleared here.
  SyncStatus status = status_;
  status.encryption_conflicts = 0;
  status.hierarchy_conflicts = 0;
  status.server_conflicts = 0;
  status.committed_count = 0;
  status.updates_available = 0;
  return status;
}

SyncStatus AllStatus::CalcSyncing(const SyncEngineEvent &event) const {
  SyncStatus status = CreateBlankStatus();
  const sessions::SyncSessionSnapshot& snapshot = event.snapshot;
  status.encryption_conflicts = snapshot.num_encryption_conflicts();
  status.hierarchy_conflicts = snapshot.num_hierarchy_conflicts();
  status.server_conflicts = snapshot.num_server_conflicts();
  status.committed_count =
      snapshot.model_neutral_state().num_successful_commits;

  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_BEGIN) {
    status.syncing = true;
  } else if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    status.syncing = false;
  }

  status.updates_available += snapshot.num_server_changes_remaining();
  status.sync_protocol_error =
      snapshot.model_neutral_state().sync_protocol_error;

  status.num_entries_by_type = snapshot.num_entries_by_type();
  status.num_to_delete_entries_by_type =
      snapshot.num_to_delete_entries_by_type();

  // Accumulate update count only once per session to avoid double-counting.
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    status.updates_received +=
        snapshot.model_neutral_state().num_updates_downloaded_total;
    status.tombstone_updates_received +=
        snapshot.model_neutral_state().num_tombstone_updates_downloaded_total;
    status.reflected_updates_received +=
        snapshot.model_neutral_state().num_reflected_updates_downloaded_total;
    status.num_commits_total +=
        snapshot.model_neutral_state().num_successful_commits;
    status.num_local_overwrites_total +=
        snapshot.model_neutral_state().num_local_overwrites;
    status.num_server_overwrites_total +=
        snapshot.model_neutral_state().num_server_overwrites;
    if (snapshot.model_neutral_state().num_updates_downloaded_total == 0) {
      ++status.empty_get_updates;
    } else {
      ++status.nonempty_get_updates;
    }
    if (snapshot.model_neutral_state().num_successful_commits == 0) {
      ++status.sync_cycles_without_commits;
    } else {
      ++status.sync_cycles_with_commits;
    }
    if (snapshot.model_neutral_state().num_successful_commits == 0 &&
        snapshot.model_neutral_state().num_updates_downloaded_total == 0) {
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
       break;
    case SyncEngineEvent::ACTIONABLE_ERROR:
      status_ = CreateBlankStatus();
      status_.sync_protocol_error =
          event.snapshot.model_neutral_state().sync_protocol_error;
      break;
    case SyncEngineEvent::RETRY_TIME_CHANGED:
      status_.retry_time = event.retry_time;
      break;
    default:
      LOG(ERROR) << "Unrecognized Syncer Event: " << event.what_happened;
      break;
  }
}

SyncStatus AllStatus::status() const {
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

void AllStatus::SetEncryptedTypes(ModelTypeSet types) {
  ScopedStatusLock lock(this);
  status_.encrypted_types = types;
}

void AllStatus::SetThrottledTypes(ModelTypeSet types) {
  ScopedStatusLock lock(this);
  status_.throttled_types = types;
}

void AllStatus::SetCryptographerReady(bool ready) {
  ScopedStatusLock lock(this);
  status_.cryptographer_ready = ready;
}

void AllStatus::SetCryptoHasPendingKeys(bool has_pending_keys) {
  ScopedStatusLock lock(this);
  status_.crypto_has_pending_keys = has_pending_keys;
}

void AllStatus::SetPassphraseType(PassphraseType type) {
  ScopedStatusLock lock(this);
  status_.passphrase_type = type;
}

void AllStatus::SetHasKeystoreKey(bool has_keystore_key) {
  ScopedStatusLock lock(this);
  status_.has_keystore_key = has_keystore_key;
}

void AllStatus::SetKeystoreMigrationTime(const base::Time& migration_time) {
  ScopedStatusLock lock(this);
  status_.keystore_migration_time = migration_time;
}

void AllStatus::SetSyncId(const std::string& sync_id) {
  ScopedStatusLock lock(this);
  status_.sync_id = sync_id;
}

void AllStatus::SetInvalidatorClientId(
    const std::string& invalidator_client_id) {
  ScopedStatusLock lock(this);
  status_.invalidator_client_id = invalidator_client_id;
}

void AllStatus::IncrementNudgeCounter(NudgeSource source) {
  ScopedStatusLock lock(this);
  switch(source) {
    case NUDGE_SOURCE_LOCAL_REFRESH:
      status_.nudge_source_local_refresh++;
      return;
    case NUDGE_SOURCE_LOCAL:
      status_.nudge_source_local++;
      return;
    case NUDGE_SOURCE_NOTIFICATION:
      status_.nudge_source_notification++;
      return;
    case NUDGE_SOURCE_UNKNOWN:
      break;
  }
  // If we're here, the source is most likely
  // NUDGE_SOURCE_UNKNOWN. That shouldn't happen.
  NOTREACHED();
}

ScopedStatusLock::ScopedStatusLock(AllStatus* allstatus)
    : allstatus_(allstatus) {
  allstatus->mutex_.Acquire();
}

ScopedStatusLock::~ScopedStatusLock() {
  allstatus_->mutex_.Release();
}

}  // namespace syncer
