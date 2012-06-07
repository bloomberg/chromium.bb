// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SESSION_SNAPSHOT_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SESSION_SNAPSHOT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "sync/internal_api/public/sessions/error_counters.h"
#include "sync/internal_api/public/sessions/sync_source_info.h"
#include "sync/internal_api/public/sessions/syncer_status.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/internal_api/public/syncable/model_type_payload_map.h"

namespace base {
class DictionaryValue;
}

namespace browser_sync {
namespace sessions {

// An immutable snapshot of state from a SyncSession.  Convenient to use as
// part of notifications as it is inherently thread-safe.
// TODO(zea): if copying this all over the place starts getting expensive,
// consider passing around immutable references instead of values.
// Default copy and assign welcome.
class SyncSessionSnapshot {
 public:
  SyncSessionSnapshot();
  SyncSessionSnapshot(
      const SyncerStatus& syncer_status,
      const ErrorCounters& errors,
      int64 num_server_changes_remaining,
      bool is_share_usable,
      syncable::ModelTypeSet initial_sync_ended,
      const syncable::ModelTypePayloadMap& download_progress_markers,
      bool more_to_sync,
      bool is_silenced,
      int num_encryption_conflicts,
      int num_hierarchy_conflicts,
      int num_simple_conflicts,
      int num_server_conflicts,
      const SyncSourceInfo& source,
      bool notifications_enabled,
      size_t num_entries,
      base::Time sync_start_time,
      bool retry_scheduled);
  ~SyncSessionSnapshot();

  // Caller takes ownership of the returned dictionary.
  base::DictionaryValue* ToValue() const;

  std::string ToString() const;

  SyncerStatus syncer_status() const;
  ErrorCounters errors() const;
  int64 num_server_changes_remaining() const;
  bool is_share_usable() const;
  syncable::ModelTypeSet initial_sync_ended() const;
  syncable::ModelTypePayloadMap download_progress_markers() const;
  bool has_more_to_sync() const;
  bool is_silenced() const;
  int num_encryption_conflicts() const;
  int num_hierarchy_conflicts() const;
  int num_simple_conflicts() const;
  int num_server_conflicts() const;
  bool did_commit_items() const;
  SyncSourceInfo source() const;
  bool notifications_enabled() const;
  size_t num_entries() const;
  base::Time sync_start_time() const;
  bool retry_scheduled() const;

 private:
  SyncerStatus syncer_status_;
  ErrorCounters errors_;
  int64 num_server_changes_remaining_;
  bool is_share_usable_;
  syncable::ModelTypeSet initial_sync_ended_;
  syncable::ModelTypePayloadMap download_progress_markers_;
  bool has_more_to_sync_;
  bool is_silenced_;
  int num_encryption_conflicts_;
  int num_hierarchy_conflicts_;
  int num_simple_conflicts_;
  int num_server_conflicts_;
  SyncSourceInfo source_;
  bool notifications_enabled_;
  size_t num_entries_;
  base::Time sync_start_time_;
  bool retry_scheduled_;
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SESSION_SNAPSHOT_H_
