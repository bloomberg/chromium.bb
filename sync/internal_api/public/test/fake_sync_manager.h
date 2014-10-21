// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/test/null_sync_context_proxy.h"
#include "sync/internal_api/public/test/test_user_share.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace syncer {

class FakeSyncEncryptionHandler;

class FakeSyncManager : public SyncManager {
 public:
  // |initial_sync_ended_types|: The set of types that have initial_sync_ended
  // set to true. This value will be used by InitialSyncEndedTypes() until the
  // next configuration is performed.
  //
  // |progress_marker_types|: The set of types that have valid progress
  // markers. This will be used by GetTypesWithEmptyProgressMarkerToken() until
  // the next configuration is performed.
  //
  // |configure_fail_types|: The set of types that will fail
  // configuration. Once ConfigureSyncer is called, the
  // |initial_sync_ended_types_| and |progress_marker_types_| will be updated
  // to include those types that didn't fail.
  FakeSyncManager(ModelTypeSet initial_sync_ended_types,
                  ModelTypeSet progress_marker_types,
                  ModelTypeSet configure_fail_types);
  ~FakeSyncManager() override;

  // Returns those types that have been cleaned (purged from the directory)
  // since the last call to GetAndResetCleanedTypes(), or since startup if never
  // called.
  ModelTypeSet GetAndResetCleanedTypes();

  // Returns those types that have been downloaded since the last call to
  // GetAndResetDownloadedTypes(), or since startup if never called.
  ModelTypeSet GetAndResetDownloadedTypes();

  // Returns those types that have been marked as enabled since the
  // last call to GetAndResetEnabledTypes(), or since startup if never
  // called.
  ModelTypeSet GetAndResetEnabledTypes();

  // Returns the types that have most recently received a refresh request.
  ModelTypeSet GetLastRefreshRequestTypes();

  // Returns the most recent configuration reason since the last call to
  // GetAndResetConfigureReason, or since startup if never called.
  ConfigureReason GetAndResetConfigureReason();

  // Posts a method to invalidate the given IDs on the sync thread.
  void OnIncomingInvalidation(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> interface) override;

  // Posts a method to update the invalidator state on the sync thread.
  void SetInvalidatorEnabled(bool invalidator_enabled) override;

  // Block until the sync thread has finished processing any pending messages.
  void WaitForSyncThread();

  // SyncManager implementation.
  // Note: we treat whatever message loop this is called from as the sync
  // loop for purposes of callbacks.
  void Init(InitArgs* args) override;
  ModelTypeSet InitialSyncEndedTypes() override;
  ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) override;
  bool PurgePartiallySyncedTypes() override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void StartSyncingNormally(const ModelSafeRoutingInfo& routing_info) override;
  void ConfigureSyncer(ConfigureReason reason,
                       ModelTypeSet to_download,
                       ModelTypeSet to_purge,
                       ModelTypeSet to_journal,
                       ModelTypeSet to_unapply,
                       const ModelSafeRoutingInfo& new_routing_info,
                       const base::Closure& ready_task,
                       const base::Closure& retry_task) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  SyncStatus GetDetailedStatus() const override;
  void SaveChanges() override;
  void ShutdownOnSyncThread(ShutdownReason reason) override;
  UserShare* GetUserShare() override;
  syncer::SyncContextProxy* GetSyncContextProxy() override;
  const std::string cache_guid() override;
  bool ReceivedExperiment(Experiments* experiments) override;
  bool HasUnsyncedItems() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  ScopedVector<syncer::ProtocolEvent> GetBufferedProtocolEvents() override;
  scoped_ptr<base::ListValue> GetAllNodesForType(
      syncer::ModelType type) override;
  void RefreshTypes(ModelTypeSet types) override;
  void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RequestEmitDebugInfo() override;

 private:
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  ObserverList<SyncManager::Observer> observers_;

  // Faked directory state.
  ModelTypeSet initial_sync_ended_types_;
  ModelTypeSet progress_marker_types_;

  // Test specific state.
  // The types that should fail configuration attempts. These types will not
  // have their progress markers or initial_sync_ended bits set.
  ModelTypeSet configure_fail_types_;
  // The set of types that have been cleaned up.
  ModelTypeSet cleaned_types_;
  // The set of types that have been downloaded.
  ModelTypeSet downloaded_types_;
  // The set of types that have been enabled.
  ModelTypeSet enabled_types_;

  // The types for which a refresh was most recently requested.
  ModelTypeSet last_refresh_request_types_;

  // The most recent configure reason.
  ConfigureReason last_configure_reason_;

  scoped_ptr<FakeSyncEncryptionHandler> fake_encryption_handler_;

  TestUserShare test_user_share_;

  NullSyncContextProxy null_sync_context_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncManager);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_SYNC_MANAGER_H_
