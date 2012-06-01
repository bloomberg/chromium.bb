// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session_context.h"

#include "sync/sessions/debug_info_getter.h"
#include "sync/util/extensions_activity_monitor.h"

namespace browser_sync {
namespace sessions {

const unsigned int kMaxMessagesToRecord = 10;
const unsigned int kMaxMessageSizeToRecord = 5 * 1024;

SyncSessionContext::SyncSessionContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    const ModelSafeRoutingInfo& model_safe_routing_info,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivityMonitor* extensions_activity_monitor,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    browser_sync::TrafficRecorder* traffic_recorder)
    : resolver_(NULL),
      connection_manager_(connection_manager),
      directory_(directory),
      routing_info_(model_safe_routing_info),
      workers_(workers),
      extensions_activity_monitor_(extensions_activity_monitor),
      notifications_enabled_(false),
      max_commit_batch_size_(kDefaultMaxCommitBatchSize),
      debug_info_getter_(debug_info_getter),
      traffic_recorder_(traffic_recorder) {
  std::vector<SyncEngineEventListener*>::const_iterator it;
  for (it = listeners.begin(); it != listeners.end(); ++it)
    listeners_.AddObserver(*it);
}

SyncSessionContext::SyncSessionContext()
    : connection_manager_(NULL),
      directory_(NULL),
      extensions_activity_monitor_(NULL),
      debug_info_getter_(NULL),
      traffic_recorder_(NULL) {
}

SyncSessionContext::~SyncSessionContext() {
}

void SyncSessionContext::SetUnthrottleTime(syncable::ModelTypeSet types,
                                           const base::TimeTicks& time) {
  for (syncable::ModelTypeSet::Iterator it = types.First();
       it.Good(); it.Inc()) {
    unthrottle_times_[it.Get()] = time;
  }
}

void SyncSessionContext::PruneUnthrottledTypes(const base::TimeTicks& time) {
  UnthrottleTimes::iterator it = unthrottle_times_.begin();
  while (it != unthrottle_times_.end()) {
    if (it->second <= time) {
      // Delete and increment the iterator.
      UnthrottleTimes::iterator iterator_to_delete = it;
      ++it;
      unthrottle_times_.erase(iterator_to_delete);
    } else {
      // Just increment the iterator.
      ++it;
    }
  }
}

// TODO(lipalani): Call this function and fill the return values in snapshot
// so it could be shown in the about:sync page.
syncable::ModelTypeSet SyncSessionContext::GetThrottledTypes() const {
  syncable::ModelTypeSet types;
  for (UnthrottleTimes::const_iterator it = unthrottle_times_.begin();
       it != unthrottle_times_.end();
       ++it) {
    types.Put(it->first);
  }
  return types;
}

}  // namespace sessions
}  // namespace browser_sync
