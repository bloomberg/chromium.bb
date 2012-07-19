// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// InternalComponentsFactory exists so that tests can override creation of
// components used by the SyncManager that are not exposed across the sync
// API boundary.

#ifndef SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_H_
#define SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace syncer {

class ExtensionsActivityMonitor;
class ServerConnectionManager;
class SyncEngineEventListener;
class SyncScheduler;
class TrafficRecorder;

class ThrottledDataTypeTracker;

namespace sessions {
class DebugInfoGetter;
class SyncSessionContext;
}

namespace syncable {
class Directory;
class DirectoryBackingStore;
}

class InternalComponentsFactory {
 public:
  virtual ~InternalComponentsFactory() {}

  virtual scoped_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context) = 0;

  virtual scoped_ptr<sessions::SyncSessionContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      const ModelSafeRoutingInfo& routing_info,
      const std::vector<ModelSafeWorker*> workers,
      ExtensionsActivityMonitor* monitor,
      ThrottledDataTypeTracker* throttled_data_type_tracker,
      const std::vector<SyncEngineEventListener*>& listeners,
      sessions::DebugInfoGetter* debug_info_getter,
      syncer::TrafficRecorder* traffic_recorder) = 0;

  virtual scoped_ptr<syncable::DirectoryBackingStore>
  BuildDirectoryBackingStore(
      const std::string& dir_name,
      const FilePath& backing_filepath) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_H_
