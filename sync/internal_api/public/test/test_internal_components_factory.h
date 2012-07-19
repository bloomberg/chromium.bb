// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_

#include "sync/internal_api/public/internal_components_factory.h"

namespace syncer {

class TestInternalComponentsFactory : public InternalComponentsFactory {
 public:
  enum StorageOption {
    // BuildDirectoryBackingStore should not use persistent on-disk storage.
    IN_MEMORY,
    // Use this if you want BuildDirectoryBackingStore to create a real
    // on disk store.
    ON_DISK
  };

  explicit TestInternalComponentsFactory(StorageOption option);
  virtual ~TestInternalComponentsFactory();

  virtual scoped_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context) OVERRIDE;

  virtual scoped_ptr<sessions::SyncSessionContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      const ModelSafeRoutingInfo& routing_info,
      const std::vector<ModelSafeWorker*> workers,
      ExtensionsActivityMonitor* monitor,
      ThrottledDataTypeTracker* throttled_data_type_tracker,
      const std::vector<SyncEngineEventListener*>& listeners,
      sessions::DebugInfoGetter* debug_info_getter,
      syncer::TrafficRecorder* traffic_recorder) OVERRIDE;

  virtual scoped_ptr<syncable::DirectoryBackingStore>
  BuildDirectoryBackingStore(
      const std::string& dir_name,
      const FilePath& backing_filepath) OVERRIDE;

 private:
  const StorageOption storage_option_;

  DISALLOW_COPY_AND_ASSIGN(TestInternalComponentsFactory);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_
