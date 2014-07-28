// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_SYNC_WORKER_IMPL_H_
#define SYNC_ENGINE_MODEL_TYPE_SYNC_WORKER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/threading/non_thread_safe.h"
#include "sync/base/sync_export.h"
#include "sync/engine/commit_contributor.h"
#include "sync/engine/model_type_sync_worker.h"
#include "sync/engine/nudge_handler.h"
#include "sync/engine/update_handler.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace syncer {

class ModelTypeSyncProxy;
class EntityTracker;

// A smart cache for sync types that use message passing (rather than
// transactions and the syncable::Directory) to communicate with the sync
// thread.
//
// When the non-blocking sync type wants to talk with the sync server, it will
// send a message from its thread to this object on the sync thread.  This
// object ensures the appropriate sync server communication gets scheduled and
// executed.  The response, if any, will be returned to the non-blocking sync
// type's thread eventually.
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
class SYNC_EXPORT ModelTypeSyncWorkerImpl : public UpdateHandler,
                                            public CommitContributor,
                                            public ModelTypeSyncWorker,
                                            public base::NonThreadSafe {
 public:
  ModelTypeSyncWorkerImpl(ModelType type,
                          const DataTypeState& initial_state,
                          NudgeHandler* nudge_handler,
                          scoped_ptr<ModelTypeSyncProxy> type_sync_proxy);
  virtual ~ModelTypeSyncWorkerImpl();

  ModelType GetModelType() const;

  // UpdateHandler implementation.
  virtual void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const OVERRIDE;
  virtual void GetDataTypeContext(
      sync_pb::DataTypeContext* context) const OVERRIDE;
  virtual SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status) OVERRIDE;
  virtual void ApplyUpdates(sessions::StatusController* status) OVERRIDE;
  virtual void PassiveApplyUpdates(sessions::StatusController* status) OVERRIDE;

  // ModelTypeSyncWorker implementation.
  virtual void EnqueueForCommit(
      const CommitRequestDataList& request_list) OVERRIDE;

  // CommitContributor implementation.
  virtual scoped_ptr<CommitContribution> GetContribution(
      size_t max_entries) OVERRIDE;

  // Callback for when our contribution gets a response.
  void OnCommitResponse(const CommitResponseDataList& response_list);

  base::WeakPtr<ModelTypeSyncWorkerImpl> AsWeakPtr();

 private:
  typedef std::map<std::string, EntityTracker*> EntityMap;

  // Stores a single commit request in this object's internal state.
  void StorePendingCommit(const CommitRequestData& request);

  // Returns true if all data type state required for commits is available.  In
  // practice, this means that it returns true from the time this object first
  // receives notice of a successful update fetch from the server.
  bool CanCommitItems() const;

  // Initializes the parts of a commit entity that are the responsibility of
  // this class, and not the EntityTracker.  Some fields, like the
  // client-assigned ID, can only be set by an entity with knowledge of the
  // entire data type's state.
  void HelpInitializeCommitEntity(sync_pb::SyncEntity* commit_entity);

  ModelType type_;

  // State that applies to the entire model type.
  DataTypeState data_type_state_;

  // Pointer to the ModelTypeSyncProxy associated with this worker.
  // This is NULL when no proxy is connected..
  scoped_ptr<ModelTypeSyncProxy> type_sync_proxy_;

  // Interface used to access and send nudges to the sync scheduler.  Not owned.
  NudgeHandler* nudge_handler_;

  // A map of per-entity information known to this object.
  //
  // When commits are pending, their information is stored here.  This
  // information is dropped from memory when the commit succeeds or gets
  // cancelled.
  //
  // This also stores some information related to received server state in
  // order to implement reflection blocking and conflict detection.  This
  // information is kept in memory indefinitely.  With a bit more coordination
  // with the model thread, we could optimize this to reduce memory usage in
  // the steady state.
  EntityMap entities_;
  STLValueDeleter<EntityMap> entities_deleter_;

  base::WeakPtrFactory<ModelTypeSyncWorkerImpl> weak_ptr_factory_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_MODEL_TYPE_SYNC_WORKER_IMPL_H_
