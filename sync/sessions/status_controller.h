// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatusController handles all counter and status related number crunching and
// state tracking on behalf of a SyncSession.
//
// This object may be accessed from many different threads.  It will be accessed
// most often from the syncer thread.  However, when update application is in
// progress it may also be accessed from the worker threads.  This is safe
// because only one of them will run at a time, and the syncer thread will be
// blocked until update application completes.
//
// This object contains only global state.  None of its members are per model
// type counters.

#ifndef SYNC_SESSIONS_STATUS_CONTROLLER_H_
#define SYNC_SESSIONS_STATUS_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"

namespace syncer {
namespace sessions {

class SYNC_EXPORT_PRIVATE StatusController {
 public:
  explicit StatusController();
  ~StatusController();

  // ClientToServer messages.
  const ModelTypeSet commit_request_types() const {
    return model_neutral_.commit_request_types;
  }
  void set_commit_request_types(ModelTypeSet value) {
    model_neutral_.commit_request_types = value;
  }

  // Various conflict counters.
  int num_encryption_conflicts() const;
  int num_hierarchy_conflicts() const;
  int num_server_conflicts() const;

  // Aggregate sum of all conflicting items over all conflict types.
  int TotalNumConflictingItems() const;

  // Number of successfully applied updates.
  int num_updates_applied() const;

  int num_server_overwrites() const;

  base::Time sync_start_time() const {
    // The time at which we sent the first GetUpdates command for this sync.
    return sync_start_time_;
  }

  const ModelNeutralState& model_neutral_state() const {
    return model_neutral_;
  }

  SyncerError last_get_key_result() const;

  // Download counters.
  void increment_num_updates_downloaded_by(int value);
  void increment_num_tombstone_updates_downloaded_by(int value);
  void increment_num_reflected_updates_downloaded_by(int value);

  // Update application and conflict resolution counters.
  void increment_num_updates_applied_by(int value);
  void increment_num_encryption_conflicts_by(int value);
  void increment_num_hierarchy_conflicts_by(int value);
  void increment_num_server_conflicts();
  void increment_num_local_overwrites();
  void increment_num_server_overwrites();

  // Commit counters.
  void increment_num_successful_commits();
  void increment_num_successful_bookmark_commits();
  void set_num_successful_bookmark_commits(int value);

  // Server communication status tracking.
  void set_last_get_key_result(const SyncerError result);
  void set_last_download_updates_result(const SyncerError result);
  void set_commit_result(const SyncerError result);

  void UpdateStartTime();

 private:
  ModelNeutralState model_neutral_;

  base::Time sync_start_time_;

  DISALLOW_COPY_AND_ASSIGN(StatusController);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_STATUS_CONTROLLER_H_
