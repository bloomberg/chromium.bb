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
#include "sync/util/extensions_activity_monitor.h"

namespace syncer {

namespace sessions {
class OrderedCommitSet;
}

namespace syncable {
class Entry;
class BaseTransaction;
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
  BuildCommitCommand(
      syncable::BaseTransaction* trans,
      const sessions::OrderedCommitSet& batch_commit_set,
      sync_pb::ClientToServerMessage* commit_message,
      ExtensionsActivityMonitor::Records* extensions_activity_buffer);
  virtual ~BuildCommitCommand();

  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BuildCommitCommandTest, InterpolatePosition);

  void AddExtensionsActivityToMessage(sessions::SyncSession* session,
                                      sync_pb::CommitMessage* message);

  // Fills the config_params field of |message|.
  void AddClientConfigParamsToMessage(sessions::SyncSession* session,
                                      sync_pb::CommitMessage* message);

  DISALLOW_COPY_AND_ASSIGN(BuildCommitCommand);

  // A pointer to a valid transaction not owned by this class.
  syncable::BaseTransaction* trans_;

  // Input parameter; see constructor comment.
  const sessions::OrderedCommitSet& batch_commit_set_;

  // Output parameter; see constructor comment.
  sync_pb::ClientToServerMessage* commit_message_;

  ExtensionsActivityMonitor::Records* extensions_activity_buffer_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_
