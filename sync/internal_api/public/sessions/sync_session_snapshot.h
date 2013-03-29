// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SESSION_SNAPSHOT_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SESSION_SNAPSHOT_H_

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/progress_marker_map.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"
#include "sync/internal_api/public/sessions/sync_source_info.h"

namespace base {
class DictionaryValue;
}

namespace syncer {
namespace sessions {

// An immutable snapshot of state from a SyncSession.  Convenient to use as
// part of notifications as it is inherently thread-safe.
// TODO(zea): if copying this all over the place starts getting expensive,
// consider passing around immutable references instead of values.
// Default copy and assign welcome.
class SYNC_EXPORT SyncSessionSnapshot {
 public:
  SyncSessionSnapshot();
  SyncSessionSnapshot(
      const ModelNeutralState& model_neutral_state,
      const ProgressMarkerMap& download_progress_markers,
      bool is_silenced,
      int num_encryption_conflicts,
      int num_hierarchy_conflicts,
      int num_server_conflicts,
      const SyncSourceInfo& source,
      bool notifications_enabled,
      size_t num_entries,
      base::Time sync_start_time,
      const std::vector<int>& num_entries_by_type,
      const std::vector<int>& num_to_delete_entries_by_type);
  ~SyncSessionSnapshot();

  // Caller takes ownership of the returned dictionary.
  base::DictionaryValue* ToValue() const;

  std::string ToString() const;

  ModelNeutralState model_neutral_state() const {
    return model_neutral_state_;
  }
  int64 num_server_changes_remaining() const;
  const ProgressMarkerMap& download_progress_markers() const;
  bool is_silenced() const;
  int num_encryption_conflicts() const;
  int num_hierarchy_conflicts() const;
  int num_server_conflicts() const;
  SyncSourceInfo source() const;
  bool notifications_enabled() const;
  size_t num_entries() const;
  base::Time sync_start_time() const;
  const std::vector<int>& num_entries_by_type() const;
  const std::vector<int>& num_to_delete_entries_by_type() const;

  // Set iff this snapshot was not built using the default constructor.
  bool is_initialized() const;

 private:
  ModelNeutralState model_neutral_state_;
  ProgressMarkerMap download_progress_markers_;
  bool is_silenced_;
  int num_encryption_conflicts_;
  int num_hierarchy_conflicts_;
  int num_server_conflicts_;
  SyncSourceInfo source_;
  bool notifications_enabled_;
  size_t num_entries_;
  base::Time sync_start_time_;

  std::vector<int> num_entries_by_type_;
  std::vector<int> num_to_delete_entries_by_type_;

  bool is_initialized_;
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SESSION_SNAPSHOT_H_
