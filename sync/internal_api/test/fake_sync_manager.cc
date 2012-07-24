// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_sync_manager.h"

#include "base/message_loop.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/sync_notifier.h"

namespace syncer {

FakeSyncManager::FakeSyncManager() {
}

FakeSyncManager::~FakeSyncManager() {
}

void FakeSyncManager::set_initial_sync_ended_types(ModelTypeSet types) {
  initial_sync_ended_types_ = types;
}

void FakeSyncManager::set_progress_marker_types(ModelTypeSet types) {
  progress_marker_types_ = types;
}

void FakeSyncManager::set_configure_fail_types(ModelTypeSet types) {
  configure_fail_types_ = types;
}

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

bool FakeSyncManager::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int sync_server_port,
    bool use_ssl,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner,
    scoped_ptr<HttpPostProviderFactory> post_factory,
    const ModelSafeRoutingInfo& model_safe_routing_info,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivityMonitor* extensions_activity_monitor,
    ChangeDelegate* change_delegate,
    const SyncCredentials& credentials,
    scoped_ptr<SyncNotifier> sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    scoped_ptr<InternalComponentsFactory> internal_components_factory,
    Encryptor* encryptor,
    UnrecoverableErrorHandler* unrecoverable_error_handler,
    ReportUnrecoverableErrorFunction
        report_unrecoverable_error_function) {
  sync_loop_ = MessageLoop::current();
  PurgePartiallySyncedTypes();
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        WeakHandle<JsBackend>(),
                        true));
  return true;
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
  return true;
}

void FakeSyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::UpdateEnabledTypes(const ModelTypeSet& types) {
  enabled_types_ = types;
}

void FakeSyncManager::StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) {
  // Do nothing.
}

void FakeSyncManager::SetEncryptionPassphrase(const std::string& passphrase,
                                              bool is_explicit) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::SetDecryptionPassphrase(const std::string& passphrase) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::ConfigureSyncer(
    ConfigureReason reason,
    const ModelTypeSet& types_to_config,
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

bool FakeSyncManager::IsUsingExplicitPassphrase() {
  NOTIMPLEMENTED();
  return false;
}

void FakeSyncManager::SaveChanges() {
  // Do nothing.
}

void FakeSyncManager::StopSyncingForShutdown(const base::Closure& callback) {
  sync_loop_->PostTask(FROM_HERE, callback);
}

void FakeSyncManager::ShutdownOnSyncThread() {
  // Do nothing.
}

UserShare* FakeSyncManager::GetUserShare() {
  NOTIMPLEMENTED();
  return NULL;
}

void FakeSyncManager::RefreshNigori(const std::string& chrome_version,
                                    const base::Closure& done_callback) {
  done_callback.Run();
}

void FakeSyncManager::EnableEncryptEverything() {
  NOTIMPLEMENTED();
}

bool FakeSyncManager::ReceivedExperiment(Experiments* experiments) {
  return false;
}

bool FakeSyncManager::HasUnsyncedItems() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace syncer

