// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_NON_BLOCKING_TYPE_COMMIT_CONTRIBUTION_H_
#define SYNC_ENGINE_NON_BLOCKING_TYPE_COMMIT_CONTRIBUTION_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "sync/engine/commit_contribution.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {

class ModelTypeWorker;

// A non-blocking sync type's contribution to an outgoing commit message.
//
// Helps build a commit message and process its response.  It collaborates
// closely with the ModelTypeWorker.
class NonBlockingTypeCommitContribution : public syncer::CommitContribution {
 public:
  NonBlockingTypeCommitContribution(
      const sync_pb::DataTypeContext& context,
      const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
      ModelTypeWorker* worker);
  ~NonBlockingTypeCommitContribution() override;

  // Implementation of CommitContribution
  void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) override;
  syncer::SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      syncer::sessions::StatusController* status) override;
  void CleanUp() override;
  size_t GetNumEntries() const override;

 private:
  // A non-owned pointer back to the object that created this contribution.
  ModelTypeWorker* const worker_;

  // The type-global context information.
  const sync_pb::DataTypeContext context_;

  // The set of entities to be committed, serialized as SyncEntities.
  const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> entities_;

  // The index in the commit message where this contribution's entities are
  // added.  Used to correlate per-item requests with per-item responses.
  size_t entries_start_index_;

  // A flag used to ensure this object's contract is respected.  Helps to check
  // that CleanUp() is called before the object is destructed.
  bool cleaned_up_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingTypeCommitContribution);
};

}  // namespace syncer_v2

#endif  // SYNC_ENGINE_NON_BLOCKING_TYPE_COMMIT_CONTRIBUTION_H_
