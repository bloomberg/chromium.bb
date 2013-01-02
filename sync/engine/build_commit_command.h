// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_
#define SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "sync/base/sync_export.h"
#include "sync/engine/syncer_command.h"
#include "sync/syncable/entry_kernel.h"

namespace syncer {

namespace sessions {
class OrderedCommitSet;
}

namespace syncable {
class Entry;
}

// A class that contains the code used to serialize a set of sync items into a
// protobuf commit message.  This conversion process references the
// syncable::Directory, which is why it must be called within the same
// transaction as the GetCommitIdsCommand that fetched the set of items to be
// committed.
//
// See SyncerCommand documentation for more info.
class SYNC_EXPORT_PRIVATE BuildCommitCommand : public SyncerCommand {
 public:
  // The batch_commit_set parameter contains a set of references to the items
  // that should be committed.
  //
  // The commit_message parameter is an output parameter which will contain the
  // fully initialized commit message once ExecuteImpl() has been called.
  BuildCommitCommand(const sessions::OrderedCommitSet& batch_commit_set,
                     sync_pb::ClientToServerMessage* commit_message);
  virtual ~BuildCommitCommand();

  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BuildCommitCommandTest, InterpolatePosition);

  // Functions returning constants controlling range of values.
  static int64 GetFirstPosition();
  static int64 GetLastPosition();
  static int64 GetGap();

  void AddExtensionsActivityToMessage(sessions::SyncSession* session,
                                      sync_pb::CommitMessage* message);
  // Helper for computing position.  Find the numeric position value
  // of the closest already-synced entry.  |direction| must be one of
  // NEXT_ID or PREV_ID; this parameter controls the search direction.
  // For an open range (no predecessor or successor), the return
  // value will be kFirstPosition or kLastPosition.
  int64 FindAnchorPosition(syncable::IdField direction,
                           const syncable::Entry& entry);
  // Given two values of the type returned by FindAnchorPosition,
  // compute a third value in between the two ranges.
  int64 InterpolatePosition(int64 lo, int64 hi);

  DISALLOW_COPY_AND_ASSIGN(BuildCommitCommand);

  // Input parameter; see constructor comment.
  const sessions::OrderedCommitSet& batch_commit_set_;

  // Output parameter; see constructor comment.
  sync_pb::ClientToServerMessage* commit_message_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_
