// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_DIRECTORY_COMMIT_CONTRIBUTION_H_
#define SYNC_ENGINE_SYNC_DIRECTORY_COMMIT_CONTRIBUTION_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"

namespace syncer {

namespace sessions {
class StatusController;
}  // namespace sessions

namespace syncable {
class Directory;
}  // namespace syncable

// This class represents a set of items belonging to a particular data type that
// have been selected from the syncable Directory and prepared for commit.
//
// This class handles the bookkeeping related to the commit of these items,
// including processing the commit response message and setting and unsetting
// the SYNCING bits.
class SYNC_EXPORT_PRIVATE SyncDirectoryCommitContribution {
 public:
  // This destructor will DCHECK if UnsetSyncingBits() has not been called yet.
  ~SyncDirectoryCommitContribution();

  // Build a CommitContribution from the IS_UNSYNCED items in |dir| with the
  // given |type|.  The contribution will include at most |max_items| entries.
  //
  // This function may return NULL if this type has no items ready for and
  // requiring commit.  This function may make model neutral changes to the
  // directory.
  static SyncDirectoryCommitContribution* Build(
      syncable::Directory* dir,
      ModelType type,
      size_t max_items);

  // Serialize this contribution's entries to the given commit request |msg|.
  //
  // This function is not const.  It will update some state in this contribution
  // that will be used when processing the associated commit response.  This
  // function should not be called more than once.
  void AddToCommitMessage(sync_pb::ClientToServerMessage* msg);

  // Updates this contribution's contents in accordance with the provided
  // |response|.
  //
  // This function may make model-neutral changes to the directory.  It is not
  // valid to call this function unless AddToCommitMessage() was called earlier.
  // This function should not be called more than once.
  SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      sessions::StatusController* status);

  // Cleans up any temproary state associated with the commit.  Must be called
  // before destruction.
  void CleanUp();

  // Returns the number of entries included in this contribution.
  size_t GetNumEntries() const;

 private:
  class SyncDirectoryCommitContributionTest;
  FRIEND_TEST_ALL_PREFIXES(SyncDirectoryCommitContributionTest, GatherByTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncDirectoryCommitContributionTest,
                           GatherAndTruncate);

  SyncDirectoryCommitContribution(
      const std::vector<int64>& metahandles,
      const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
      syncable::Directory* directory);

  void UnsetSyncingBits();

  syncable::Directory* dir_;
  const std::vector<int64> metahandles_;
  const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> entities_;
  size_t entries_start_index_;

  // This flag is tracks whether or not the directory entries associated with
  // this commit still have their SYNCING bits set.  These bits will be set when
  // the CommitContribution is created with Build() and unset when CleanUp() is
  // called.  This flag must be unset by the time our destructor is called.
  bool syncing_bits_set_;

  DISALLOW_COPY_AND_ASSIGN(SyncDirectoryCommitContribution);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_DIRECTORY_COMMIT_CONTRIBUTION_H_
