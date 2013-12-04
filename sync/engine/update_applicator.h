// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An UpdateApplicator is used to iterate over a number of unapplied updates,
// applying them to the client using the given syncer session.
//
// UpdateApplicator might resemble an iterator, but it actually keeps retrying
// failed updates until no remaining updates can be successfully applied.

#ifndef SYNC_ENGINE_UPDATE_APPLICATOR_H_
#define SYNC_ENGINE_UPDATE_APPLICATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/port.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/syncable/syncable_id.h"
#include "sync/sessions/status_controller.h"

namespace syncer {

namespace sessions {
class StatusController;
}

namespace syncable {
class WriteTransaction;
class Entry;
}

class ConflictResolver;
class Cryptographer;

class UpdateApplicator {
 public:
  UpdateApplicator(Cryptographer* cryptographer);
  ~UpdateApplicator();

  // Attempt to apply the specified updates.
  void AttemptApplications(syncable::WriteTransaction* trans,
                           const std::vector<int64>& handles);

  int updates_applied() {
    return updates_applied_;
  }

  int encryption_conflicts() {
    return encryption_conflicts_;
  }

  int hierarchy_conflicts() {
    return hierarchy_conflicts_;
  }

  const std::set<syncable::Id>& simple_conflict_ids() {
    return simple_conflict_ids_;
  }

 private:
  // If true, AttemptOneApplication will skip over |entry| and return true.
  bool SkipUpdate(const syncable::Entry& entry);

  // Used to decrypt sensitive sync nodes.
  Cryptographer* cryptographer_;

  DISALLOW_COPY_AND_ASSIGN(UpdateApplicator);

  int updates_applied_;
  int encryption_conflicts_;
  int hierarchy_conflicts_;
  std::set<syncable::Id> simple_conflict_ids_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_UPDATE_APPLICATOR_H_
