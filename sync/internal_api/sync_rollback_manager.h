// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_H_
#define SYNC_INTERNAL_API_SYNC_ROLLBACK_MANAGER_H_

#include <string>
#include <vector>

#include "sync/internal_api/sync_rollback_manager_base.h"

namespace syncer {

// SyncRollbackManager restores user's data to pre-sync state using backup
// DB created by SyncBackupManager.
class SYNC_EXPORT_PRIVATE SyncRollbackManager : public SyncRollbackManagerBase {
 public:
  SyncRollbackManager();
  virtual ~SyncRollbackManager();

  // SyncManager implementation.
  virtual void Init(
      const base::FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
      ExtensionsActivity* extensions_activity,
      SyncManager::ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      const std::string& invalidator_client_id,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      InternalComponentsFactory* internal_components_factory,
      Encryptor* encryptor,
      scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      CancelationSignal* cancelation_signal) OVERRIDE;
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
