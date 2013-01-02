// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_PROCESS_COMMIT_RESPONSE_COMMAND_H_
#define SYNC_ENGINE_PROCESS_COMMIT_RESPONSE_COMMAND_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/engine/model_changing_syncer_command.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

namespace sessions {
class OrderedCommitSet;
}

namespace syncable {
class Id;
class WriteTransaction;
class MutableEntry;
class Directory;
}

// A class that processes the server's response to our commmit attempt.  Note
// that some of the preliminary processing is performed in
// PostClientToServerMessage command.
//
// As part of processing the commit response, this command will modify sync
// entries.  It can rename items, update their versions, etc.
//
// This command will return a non-SYNCER_OK value if an error occurred while
// processing the response, or if the server's response indicates that it had
// trouble processing the request.
//
// See SyncerCommand documentation for more info.
class SYNC_EXPORT_PRIVATE ProcessCommitResponseCommand
    : public ModelChangingSyncerCommand {
 public:

  // The commit_set parameter contains references to all the items which were
  // to be committed in this batch.
  //
  // The commmit_message parameter contains the message that was sent to the
  // server.
  //
  // The commit_response parameter contains the response received from the
  // server.  This may be uninitialized if we were unable to contact the server
  // or a serious error was encountered.
  ProcessCommitResponseCommand(
      const sessions::OrderedCommitSet& commit_set,
      const sync_pb::ClientToServerMessage& commit_message,
      const sync_pb::ClientToServerResponse& commit_response);
  virtual ~ProcessCommitResponseCommand();

 protected:
  // ModelChangingSyncerCommand implementation.
  virtual std::set<ModelSafeGroup> GetGroupsToChange(
      const sessions::SyncSession& session) const OVERRIDE;
  virtual SyncerError ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE;

 private:
  sync_pb::CommitResponse::ResponseType ProcessSingleCommitResponse(
      syncable::WriteTransaction* trans,
      const sync_pb::CommitResponse_EntryResponse& pb_commit_response,
      const sync_pb::SyncEntity& pb_committed_entry,
      const syncable::Id& pre_commit_id,
      std::set<syncable::Id>* deleted_folders);

  // Actually does the work of execute.
  SyncerError ProcessCommitResponse(sessions::SyncSession* session);

  void ProcessSuccessfulCommitResponse(
      const sync_pb::SyncEntity& committed_entry,
      const sync_pb::CommitResponse_EntryResponse& entry_response,
      const syncable::Id& pre_commit_id, syncable::MutableEntry* local_entry,
      bool syncing_was_set, std::set<syncable::Id>* deleted_folders);

  // Update the BASE_VERSION and SERVER_VERSION, post-commit.
  // Helper for ProcessSuccessfulCommitResponse.
  bool UpdateVersionAfterCommit(
      const sync_pb::SyncEntity& committed_entry,
      const sync_pb::CommitResponse_EntryResponse& entry_response,
      const syncable::Id& pre_commit_id,
      syncable::MutableEntry* local_entry);

  // If the server generated an ID for us during a commit, apply the new ID.
  // Helper for ProcessSuccessfulCommitResponse.
  bool ChangeIdAfterCommit(
      const sync_pb::CommitResponse_EntryResponse& entry_response,
      const syncable::Id& pre_commit_id,
      syncable::MutableEntry* local_entry);

  // Update the SERVER_ fields to reflect the server state after committing.
  // Helper for ProcessSuccessfulCommitResponse.
  void UpdateServerFieldsAfterCommit(
      const sync_pb::SyncEntity& committed_entry,
      const sync_pb::CommitResponse_EntryResponse& entry_response,
      syncable::MutableEntry* local_entry);

  // The server can override some values during a commit; the overridden values
  // are returned as fields in the CommitResponse_EntryResponse.  This method
  // stores the fields back in the client-visible (i.e. not the SERVER_* fields)
  // fields of the entry.  This should only be done if the item did not change
  // locally while the commit was in flight.
  // Helper for ProcessSuccessfulCommitResponse.
  void OverrideClientFieldsAfterCommit(
      const sync_pb::SyncEntity& committed_entry,
      const sync_pb::CommitResponse_EntryResponse& entry_response,
      syncable::MutableEntry* local_entry);

  // Helper to extract the final name from the protobufs.
  const std::string& GetResultingPostCommitName(
      const sync_pb::SyncEntity& committed_entry,
      const sync_pb::CommitResponse_EntryResponse& entry_response);

  // Helper to clean up in case of failure.
  void ClearSyncingBits(
      syncable::Directory *dir,
      const std::vector<syncable::Id>& commit_ids);

  const sessions::OrderedCommitSet& commit_set_;
  const sync_pb::ClientToServerMessage& commit_message_;
  const sync_pb::ClientToServerResponse& commit_response_;

  DISALLOW_COPY_AND_ASSIGN(ProcessCommitResponseCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_PROCESS_COMMIT_RESPONSE_COMMAND_H_
