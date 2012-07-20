// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/test_internal_components_factory.h"

#include "sync/sessions/sync_session_context.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/on_disk_directory_backing_store.h"
#include "sync/test/engine/fake_sync_scheduler.h"

namespace syncer {

TestInternalComponentsFactory::TestInternalComponentsFactory(
    StorageOption option)
    : storage_option_(option) {
}

TestInternalComponentsFactory::~TestInternalComponentsFactory() { }

scoped_ptr<SyncScheduler> TestInternalComponentsFactory::BuildScheduler(
    const std::string& name, sessions::SyncSessionContext* context) {
  return scoped_ptr<SyncScheduler>(new FakeSyncScheduler());
}

scoped_ptr<sessions::SyncSessionContext>
TestInternalComponentsFactory::BuildContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*> workers,
    ExtensionsActivityMonitor* monitor,
    ThrottledDataTypeTracker* throttled_data_type_tracker,
    const std::vector<SyncEngineEventListener*>& listeners,
    sessions::DebugInfoGetter* debug_info_getter,
    TrafficRecorder* traffic_recorder) {

  // Tests don't wire up listeners.
  std::vector<SyncEngineEventListener*> empty_listeners;
  return scoped_ptr<sessions::SyncSessionContext>(
      new sessions::SyncSessionContext(connection_manager, directory,
          routing_info, workers, monitor, throttled_data_type_tracker,
           empty_listeners, debug_info_getter, traffic_recorder));
}

scoped_ptr<syncable::DirectoryBackingStore>
TestInternalComponentsFactory::BuildDirectoryBackingStore(
      const std::string& dir_name, const FilePath& backing_filepath) {
  if (storage_option_ == IN_MEMORY) {
    return scoped_ptr<syncable::DirectoryBackingStore>(
        new syncable::InMemoryDirectoryBackingStore(dir_name));
  } else {
    return scoped_ptr<syncable::DirectoryBackingStore>(
        new syncable::OnDiskDirectoryBackingStore(dir_name,
                                                  backing_filepath));
  }
}

}  // namespace syncer
