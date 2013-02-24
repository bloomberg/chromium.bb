// Copyright 2012 The Chromium Authors. All rights reserved.
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

#include "base/files/file_path.h"
#include "sync/base/sync_export.h"
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

class SYNC_EXPORT InternalComponentsFactory {
 public:
  enum EncryptionMethod {
    ENCRYPTION_LEGACY,
    // Option to enable support for keystore key based encryption.
    ENCRYPTION_KEYSTORE
  };

  enum BackoffOverride {
    BACKOFF_NORMAL,
    // Use this value for integration testing to avoid long delays /
    // timing out tests. Uses kInitialBackoffShortRetrySeconds (see
    // polling_constants.h) for all initial retries.
    BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE
  };

  // Configuration options for internal components. This struct is expected
  // to grow and shrink over time with transient features / experiments,
  // roughly following command line flags in chrome. Implementations of
  // InternalComponentsFactory can use this information to build components
  // with appropriate bells and whistles.
  struct Switches {
    EncryptionMethod encryption_method;
    BackoffOverride backoff_override;
  };

  virtual ~InternalComponentsFactory() {}

  virtual scoped_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context) = 0;

  virtual scoped_ptr<sessions::SyncSessionContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      const std::vector<ModelSafeWorker*> workers,
      ExtensionsActivityMonitor* monitor,
      ThrottledDataTypeTracker* throttled_data_type_tracker,
      const std::vector<SyncEngineEventListener*>& listeners,
      sessions::DebugInfoGetter* debug_info_getter,
      TrafficRecorder* traffic_recorder) = 0;

  virtual scoped_ptr<syncable::DirectoryBackingStore>
  BuildDirectoryBackingStore(
      const std::string& dir_name,
      const base::FilePath& backing_filepath) = 0;

  // Returns the Switches struct that this object is using as configuration, if
  // the implementation is making use of one.
  virtual Switches GetSwitches() const = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_H_
