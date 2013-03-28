// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_sync_manager.h"

#include <cstddef>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/object_id_invalidation_map.h"
#include "sync/syncable/directory.h"
#include "sync/test/fake_sync_encryption_handler.h"

namespace syncer {

FakeSyncManager::FakeSyncManager(ModelTypeSet initial_sync_ended_types,
                                 ModelTypeSet progress_marker_types,
                                 ModelTypeSet configure_fail_types) :
    initial_sync_ended_types_(initial_sync_ended_types),
    progress_marker_types_(progress_marker_types),
    configure_fail_types_(configure_fail_types) {
  fake_encryption_handler_.reset(new FakeSyncEncryptionHandler());
}

FakeSyncManager::~FakeSyncManager() {}

ModelTypeSet FakeSyncManager::GetAndResetCleanedTypes() {
  ModelTypeSet cleaned_types = cleaned_types_;
  cleaned_types_.Clear();
  return cleaned_types;
}

ModelTypeSet FakeSyncManager::GetAndResetDownloadedTypes() {
  ModelTypeSet downloaded_types = downloaded_types_;
  downloaded_types_.Clear();
  return downloaded_types;
}

ModelTypeSet FakeSyncManager::GetAndResetEnabledTypes() {
  ModelTypeSet enabled_types = enabled_types_;
  enabled_types_.Clear();
  return enabled_types;
}

void FakeSyncManager::Invalidate(
    const ObjectIdInvalidationMap& invalidation_map) {
  if (!sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeSyncManager::InvalidateOnSyncThread,
                 base::Unretained(this), invalidation_map))) {
    NOTREACHED();
  }
}

void FakeSyncManager::UpdateInvalidatorState(InvalidatorState state) {
  if (!sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeSyncManager::UpdateInvalidatorStateOnSyncThread,
                 base::Unretained(this), state))) {
    NOTREACHED();
  }
}

void FakeSyncManager::WaitForSyncThread() {
  // Post a task to |sync_task_runner_| and block until it runs.
  base::RunLoop run_loop;
  if (!sync_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&base::DoNothing),
      run_loop.QuitClosure())) {
    NOTREACHED();
  }
  run_loop.Run();
}

void FakeSyncManager::Init(
    const base::FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int sync_server_port,
    bool use_ssl,
    scoped_ptr<HttpPostProviderFactory> post_factory,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivityMonitor* extensions_activity_monitor,
    ChangeDelegate* change_delegate,
    const SyncCredentials& credentials,
    scoped_ptr<Invalidator> invalidator,
    const std::string& invalidator_client_id,
    const std::string& restored_key_for_bootstrapping,
    const std::string& restored_keystore_key_for_bootstrapping,
    scoped_ptr<InternalComponentsFactory> internal_components_factory,
    Encryptor* encryptor,
    UnrecoverableErrorHandler* unrecoverable_error_handler,
    ReportUnrecoverableErrorFunction
        report_unrecoverable_error_function) {
  sync_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  PurgePartiallySyncedTypes();

  test_user_share_.SetUp();
  UserShare* share = test_user_share_.user_share();
  for (ModelTypeSet::Iterator it = initial_sync_ended_types_.First();
       it.Good(); it.Inc()) {
    TestUserShare::CreateRoot(it.Get(), share);
  }

  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        WeakHandle<JsBackend>(),
                        WeakHandle<DataTypeDebugInfoListener>(),
                        true, initial_sync_ended_types_));
}

void FakeSyncManager::ThrowUnrecoverableError() {
  NOTIMPLEMENTED();
}

ModelTypeSet FakeSyncManager::InitialSyncEndedTypes() {
  return initial_sync_ended_types_;
}

ModelTypeSet FakeSyncManager::GetTypesWithEmptyProgressMarkerToken(
    ModelTypeSet types) {
  ModelTypeSet empty_types = types;
  empty_types.RemoveAll(progress_marker_types_);
  return empty_types;
}

bool FakeSyncManager::PurgePartiallySyncedTypes() {
  ModelTypeSet partial_types;
  for (ModelTypeSet::Iterator i = progress_marker_types_.First();
       i.Good(); i.Inc()) {
    if (!initial_sync_ended_types_.Has(i.Get()))
      partial_types.Put(i.Get());
  }
  progress_marker_types_.RemoveAll(partial_types);
  cleaned_types_.PutAll(partial_types);
  return true;
}

void FakeSyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::UpdateEnabledTypes(ModelTypeSet types) {
  enabled_types_ = types;
}

void FakeSyncManager::RegisterInvalidationHandler(
    InvalidationHandler* handler) {
  registrar_.RegisterHandler(handler);
}

void FakeSyncManager::UpdateRegisteredInvalidationIds(
    InvalidationHandler* handler,
    const ObjectIdSet& ids) {
  registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeSyncManager::UnregisterInvalidationHandler(
    InvalidationHandler* handler) {
  registrar_.UnregisterHandler(handler);
}

void FakeSyncManager::AcknowledgeInvalidation(const invalidation::ObjectId& id,
                                              const AckHandle& ack_handle) {
  // Do nothing.
}

void FakeSyncManager::StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) {
  // Do nothing.
}

void FakeSyncManager::ConfigureSyncer(
    ConfigureReason reason,
    ModelTypeSet types_to_config,
    ModelTypeSet failed_types,
    const ModelSafeRoutingInfo& new_routing_info,
    const base::Closure& ready_task,
    const base::Closure& retry_task) {
  ModelTypeSet enabled_types = GetRoutingInfoTypes(new_routing_info);
  ModelTypeSet disabled_types = Difference(
      ModelTypeSet::All(), enabled_types);
  ModelTypeSet success_types = types_to_config;
  success_types.RemoveAll(configure_fail_types_);

  DVLOG(1) << "Faking configuration. Downloading: "
           << ModelTypeSetToString(success_types) << ". Cleaning: "
           << ModelTypeSetToString(disabled_types);

  // Update our fake directory by clearing and fake-downloading as necessary.
  UserShare* share = GetUserShare();
  share->directory->PurgeEntriesWithTypeIn(disabled_types, ModelTypeSet());
  for (ModelTypeSet::Iterator it = success_types.First(); it.Good(); it.Inc()) {
    // We must be careful to not create the same root node twice.
    if (!initial_sync_ended_types_.Has(it.Get())) {
      TestUserShare::CreateRoot(it.Get(), share);
    }
  }

  // Simulate cleaning up disabled types.
  // TODO(sync): consider only cleaning those types that were recently disabled,
  // if this isn't the first cleanup, which more accurately reflects the
  // behavior of the real cleanup logic.
  initial_sync_ended_types_.RemoveAll(disabled_types);
  progress_marker_types_.RemoveAll(disabled_types);
  cleaned_types_.PutAll(disabled_types);

  // Now simulate the actual configuration for those types that successfully
  // download + apply.
  progress_marker_types_.PutAll(success_types);
  initial_sync_ended_types_.PutAll(success_types);
  downloaded_types_.PutAll(success_types);

  ready_task.Run();
}

void FakeSyncManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

SyncStatus FakeSyncManager::GetDetailedStatus() const {
  NOTIMPLEMENTED();
  return SyncStatus();
}

void FakeSyncManager::SaveChanges() {
  // Do nothing.
}

void FakeSyncManager::StopSyncingForShutdown(const base::Closure& callback) {
  if (!sync_task_runner_->PostTask(FROM_HERE, callback)) {
    NOTREACHED();
  }
}

void FakeSyncManager::ShutdownOnSyncThread() {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  test_user_share_.TearDown();
}

UserShare* FakeSyncManager::GetUserShare() {
  return test_user_share_.user_share();
}

const std::string FakeSyncManager::cache_guid() {
  return test_user_share_.user_share()->directory->cache_guid();
}

bool FakeSyncManager::ReceivedExperiment(Experiments* experiments) {
  return false;
}

bool FakeSyncManager::HasUnsyncedItems() {
  NOTIMPLEMENTED();
  return false;
}

SyncEncryptionHandler* FakeSyncManager::GetEncryptionHandler() {
  return fake_encryption_handler_.get();
}

void FakeSyncManager::RefreshTypes(ModelTypeSet types) {
  last_refresh_request_types_ = types;
}

void FakeSyncManager::InvalidateOnSyncThread(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

void FakeSyncManager::UpdateInvalidatorStateOnSyncThread(
    InvalidatorState state) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  registrar_.UpdateInvalidatorState(state);
}

ModelTypeSet FakeSyncManager::GetLastRefreshRequestTypes() {
  return last_refresh_request_types_;
}

}  // namespace syncer
