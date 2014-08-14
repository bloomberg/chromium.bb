// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager.h"

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "url/gurl.h"

namespace syncer {

SyncRollbackManager::SyncRollbackManager()
    : change_delegate_(NULL) {
}

SyncRollbackManager::~SyncRollbackManager() {
}

void SyncRollbackManager::Init(InitArgs* args) {
  if (SyncRollbackManagerBase::InitInternal(
          args->database_location,
          args->internal_components_factory.get(),
          args->unrecoverable_error_handler.Pass(),
          args->report_unrecoverable_error_function)) {
    change_delegate_ = args->change_delegate;

    for (size_t i = 0; i < args->workers.size(); ++i) {
      ModelSafeGroup group = args->workers[i]->GetModelSafeGroup();
      CHECK(workers_.find(group) == workers_.end());
      workers_[group] = args->workers[i];
    }

    rollback_ready_types_ = GetUserShare()->directory->InitialSyncEndedTypes();
    rollback_ready_types_.RetainAll(BackupTypes());
  }
}

void SyncRollbackManager::StartSyncingNormally(
    const ModelSafeRoutingInfo& routing_info){
  if (rollback_ready_types_.Empty()) {
    NotifyRollbackDone();
    return;
  }

  std::map<ModelType, syncable::Directory::Metahandles> to_delete;
  {
    WriteTransaction trans(FROM_HERE, GetUserShare());
    syncable::Directory::Metahandles unsynced;
    GetUserShare()->directory->GetUnsyncedMetaHandles(trans.GetWrappedTrans(),
                                                      &unsynced);
    for (size_t i = 0; i < unsynced.size(); ++i) {
      syncable::MutableEntry e(trans.GetWrappedWriteTrans(),
                               syncable::GET_BY_HANDLE, unsynced[i]);
      if (!e.good() || e.GetIsDel() || e.GetId().ServerKnows())
        continue;

      // TODO(haitaol): roll back entries that are backed up but whose content
      //                is merged with local model during association.

      ModelType type = GetModelTypeFromSpecifics(e.GetSpecifics());
      if (!rollback_ready_types_.Has(type))
        continue;

      to_delete[type].push_back(unsynced[i]);
    }
  }

  for (std::map<ModelType, syncable::Directory::Metahandles>::iterator it =
      to_delete.begin(); it != to_delete.end(); ++it) {
    ModelSafeGroup group = routing_info.find(it->first)->second;
    CHECK(workers_.find(group) != workers_.end());
    workers_[group]->DoWorkAndWaitUntilDone(
        base::Bind(&SyncRollbackManager::DeleteOnWorkerThread,
                   base::Unretained(this),
                   it->first, it->second));
  }

  NotifyRollbackDone();
}

SyncerError SyncRollbackManager::DeleteOnWorkerThread(
    ModelType type, std::vector<int64> handles) {
  CHECK(change_delegate_);

  {
    ChangeRecordList deletes;
    WriteTransaction trans(FROM_HERE, GetUserShare());
    for (size_t i = 0; i < handles.size(); ++i) {
      syncable::MutableEntry e(trans.GetWrappedWriteTrans(),
                               syncable::GET_BY_HANDLE, handles[i]);
      if (!e.good() || e.GetIsDel())
        continue;

      ChangeRecord del;
      del.action = ChangeRecord::ACTION_DELETE;
      del.id = handles[i];
      del.specifics = e.GetSpecifics();
      deletes.push_back(del);
    }

    change_delegate_->OnChangesApplied(type, 1, &trans,
                                       MakeImmutable(&deletes));
  }

  change_delegate_->OnChangesComplete(type);
  return SYNCER_OK;
}

void SyncRollbackManager::NotifyRollbackDone() {
  SyncProtocolError error;
  error.action = ROLLBACK_DONE;
  FOR_EACH_OBSERVER(SyncManager::Observer, *GetObservers(),
                    OnActionableError(error));
}

}  // namespace syncer
