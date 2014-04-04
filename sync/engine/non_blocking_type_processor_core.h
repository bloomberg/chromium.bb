// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_NON_BLOCKING_TYPE_PROCESSOR_CORE_H_
#define SYNC_ENGINE_NON_BLOCKING_TYPE_PROCESSOR_CORE_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "sync/base/sync_export.h"
#include "sync/engine/commit_contributor.h"
#include "sync/engine/update_handler.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace syncer {

class NonBlockingTypeProcessor;

// A smart cache for sync types that use message passing (rather than
// transactions and the syncable::Directory) to communicate with the sync
// thread.
//
// When the non-blocking sync type wants to talk with the sync server, it will
// send a message from its thread to this object on the sync thread.  This
// object is responsible for helping to ensure the appropriate sync server
// communication gets scheduled and executed.  The response, if any, will be
// returned to the non-blocking sync type's thread eventually.
//
// This object also has a role to play in communications in the opposite
// direction.  Sometimes the sync thread will receive changes from the sync
// server and deliver them here.  This object will post this information back to
// the appropriate component on the model type's thread.
//
// This object does more than just pass along messages.  It understands the sync
// protocol, and it can make decisions when it sees conflicting messages.  For
// example, if the sync server sends down an update for a sync entity that is
// currently pending for commit, this object will detect this condition and
// cancel the pending commit.
class SYNC_EXPORT NonBlockingTypeProcessorCore
    : public UpdateHandler,
      public CommitContributor,
      public base::NonThreadSafe {
 public:
  NonBlockingTypeProcessorCore(
      ModelType type,
      scoped_refptr<base::SequencedTaskRunner> processor_task_runner,
      base::WeakPtr<NonBlockingTypeProcessor> processor);
  virtual ~NonBlockingTypeProcessorCore();

  ModelType GetModelType() const;

  // UpdateHandler implementation.
  virtual void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const OVERRIDE;
  virtual void GetDataTypeContext(sync_pb::DataTypeContext* context) const
      OVERRIDE;
  virtual void ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status) OVERRIDE;
  virtual void ApplyUpdates(sessions::StatusController* status) OVERRIDE;
  virtual void PassiveApplyUpdates(sessions::StatusController* status) OVERRIDE;

  // CommitContributor implementation.
  virtual scoped_ptr<CommitContribution> GetContribution(
      size_t max_entries) OVERRIDE;

  base::WeakPtr<NonBlockingTypeProcessorCore> AsWeakPtr();

 private:
  ModelType type_;
  sync_pb::DataTypeProgressMarker progress_marker_;

  scoped_refptr<base::SequencedTaskRunner> processor_task_runner_;
  base::WeakPtr<NonBlockingTypeProcessor> processor_;

  base::WeakPtrFactory<NonBlockingTypeProcessorCore> weak_ptr_factory_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_NON_BLOCKING_TYPE_PROCESSOR_CORE_H_
