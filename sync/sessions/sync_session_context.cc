// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session_context.h"

#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/sessions/debug_info_getter.h"
#include "sync/util/extensions_activity_monitor.h"

namespace csync {
namespace sessions {

const unsigned int kMaxMessagesToRecord = 10;
const unsigned int kMaxMessageSizeToRecord = 5 * 1024;

SyncSessionContext::SyncSessionContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    const ModelSafeRoutingInfo& model_safe_routing_info,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivityMonitor* extensions_activity_monitor,
    ThrottledDataTypeTracker* throttled_data_type_tracker,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    csync::TrafficRecorder* traffic_recorder)
    : resolver_(NULL),
      connection_manager_(connection_manager),
      directory_(directory),
      routing_info_(model_safe_routing_info),
      workers_(workers),
      extensions_activity_monitor_(extensions_activity_monitor),
      notifications_enabled_(false),
      max_commit_batch_size_(kDefaultMaxCommitBatchSize),
      throttled_data_type_tracker_(throttled_data_type_tracker),
      debug_info_getter_(debug_info_getter),
      traffic_recorder_(traffic_recorder) {
  std::vector<SyncEngineEventListener*>::const_iterator it;
  for (it = listeners.begin(); it != listeners.end(); ++it)
    listeners_.AddObserver(*it);
}

SyncSessionContext::~SyncSessionContext() {
}

}  // namespace sessions
}  // namespace csync
