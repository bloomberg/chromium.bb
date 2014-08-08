// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_H_
#define SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_H_

#include <string>
#include <vector>

#include "sync/internal_api/sync_rollback_manager_base.h"

class GURL;

namespace syncer {

// SyncRollbackManager restores user's data to pre-sync state using backup
// DB created by SyncBackupManager.
class SYNC_EXPORT_PRIVATE SyncRollbackManager : public SyncRollbackManagerBase {
 public:
  SyncRollbackManager();
  virtual ~SyncRollbackManager();

  // SyncManager implementation.
  virtual void Init(InitArgs* args) OVERRIDE;
  virtual void StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) OVERRIDE;

 private:
  // Deletes specified entries in local model.
  SyncerError DeleteOnWorkerThread(ModelType type, std::vector<int64> handles);

  void NotifyRollbackDone();

  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> > workers_;

  SyncManager::ChangeDelegate* change_delegate_;

  // Types that can be rolled back.
  ModelTypeSet rollback_ready_types_;

  DISALLOW_COPY_AND_ASSIGN(SyncRollbackManager);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_H_
