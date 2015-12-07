// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_COMMIT_CONTRIBUTION_H_
#define SYNC_ENGINE_COMMIT_CONTRIBUTION_H_

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"

namespace syncer {

namespace sessions {
class StatusController;
}  // namespace sessions

// This class represents a set of items belonging to a particular data type that
// have been selected from a CommitContributor and prepared for commit.
//
// This class handles the bookkeeping related to the commit of these items.
class SYNC_EXPORT_PRIVATE CommitContribution {
 public:
  CommitContribution();
  virtual ~CommitContribution() = 0;

  // Serialize this contribution's entries to the given commit request |msg|.
  //
  // This function is not const.  It may update some state in this contribution
  // that will be used when processing the associated commit response.  This
  // function should not be called more than once.
  virtual void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) = 0;

  // Updates this contribution's contents in accordance with the provided
  // |response|.
  //
  // It is not valid to call this function unless AddToCommitMessage() was
  // called earlier.  This function should not be called more than once.
  virtual SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      sessions::StatusController* status) = 0;

  // Cleans up any temproary state associated with the commit.  Must be called
  // before destruction.
  virtual void CleanUp() = 0;

  // Returns the number of entries included in this contribution.
  virtual size_t GetNumEntries() const = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_COMMIT_CONTRIBUTION_H_
