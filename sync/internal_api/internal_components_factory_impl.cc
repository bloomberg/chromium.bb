// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/internal_components_factory_impl.h"

#include "sync/engine/backoff_delay_provider.h"
#include "sync/engine/syncer.h"
#include "sync/engine/sync_scheduler_impl.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/syncable/on_disk_directory_backing_store.h"

using base::TimeDelta;

namespace syncer {

InternalComponentsFactoryImpl::InternalComponentsFactoryImpl(
    const Switches& switches) : switches_(switches) {
}

InternalComponentsFactoryImpl::~InternalComponentsFactoryImpl() { }

scoped_ptr<SyncScheduler> InternalComponentsFactoryImpl::BuildScheduler(
    const std::string& name, sessions::SyncSessionContext* context) {

  scoped_ptr<BackoffDelayProvider> delay(BackoffDelayProvider::FromDefaults());

  if (switches_.backoff_override == BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE)
    delay.reset(BackoffDelayProvider::WithShortInitialRetryOverride());

  return scoped_ptr<SyncScheduler>(
      new SyncSchedulerImpl(name, delay.release(), context, new Syncer()));
}

scoped_ptr<sessions::SyncSessionContext>
InternalComponentsFactoryImpl::BuildContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    const std::vector<ModelSafeWorker*> workers,
    ExtensionsActivityMonitor* monitor,
    ThrottledDataTypeTracker* throttled_data_type_tracker,
    const std::vector<SyncEngineEventListener*>& listeners,
    sessions::DebugInfoGetter* debug_info_getter,
    TrafficRecorder* traffic_recorder,
    const std::string& invalidation_client_id) {
  return scoped_ptr<sessions::SyncSessionContext>(
      new sessions::SyncSessionContext(
          connection_manager, directory, workers, monitor,
          throttled_data_type_tracker, listeners, debug_info_getter,
          traffic_recorder,
          switches_.encryption_method == ENCRYPTION_KEYSTORE,
          invalidation_client_id));
}

scoped_ptr<syncable::DirectoryBackingStore>
InternalComponentsFactoryImpl::BuildDirectoryBackingStore(
      const std::string& dir_name, const base::FilePath& backing_filepath) {
  return scoped_ptr<syncable::DirectoryBackingStore>(
      new syncable::OnDiskDirectoryBackingStore(dir_name, backing_filepath));
}

InternalComponentsFactory::Switches
InternalComponentsFactoryImpl::GetSwitches() const {
  return switches_;
}

}  // namespace syncer
