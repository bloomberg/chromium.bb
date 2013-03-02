// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session_context.h"

#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/sessions/debug_info_getter.h"
#include "sync/util/extensions_activity_monitor.h"

namespace syncer {
namespace sessions {

const unsigned int kMaxMessagesToRecord = 10;
const unsigned int kMaxMessageSizeToRecord = 5 * 1024;

SyncSessionContext::SyncSessionContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivityMonitor* extensions_activity_monitor,
    ThrottledDataTypeTracker* throttled_data_type_tracker,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    TrafficRecorder* traffic_recorder,
    bool keystore_encryption_enabled,
    const std::string& invalidator_client_id)
    : connection_manager_(connection_manager),
      directory_(directory),
      workers_(workers),
      extensions_activity_monitor_(extensions_activity_monitor),
      notifications_enabled_(false),
      max_commit_batch_size_(kDefaultMaxCommitBatchSize),
      throttled_data_type_tracker_(throttled_data_type_tracker),
      debug_info_getter_(debug_info_getter),
      traffic_recorder_(traffic_recorder),
      keystore_encryption_enabled_(keystore_encryption_enabled),
      invalidator_client_id_(invalidator_client_id) {
  std::vector<SyncEngineEventListener*>::const_iterator it;
  for (it = listeners.begin(); it != listeners.end(); ++it)
    listeners_.AddObserver(*it);
}

SyncSessionContext::~SyncSessionContext() {
}

}  // namespace sessions
}  // namespace syncer
