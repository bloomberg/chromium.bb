// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session_context.h"

#include "sync/sessions/debug_info_getter.h"
#include "sync/util/extensions_activity.h"

namespace syncer {
namespace sessions {

SyncSessionContext::SyncSessionContext(
    ServerConnectionManager* connection_manager,
    syncable::Directory* directory,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivity* extensions_activity,
    const std::vector<SyncEngineEventListener*>& listeners,
    DebugInfoGetter* debug_info_getter,
    TrafficRecorder* traffic_recorder,
    bool keystore_encryption_enabled,
    bool client_enabled_pre_commit_update_avoidance,
    const std::string& invalidator_client_id)
    : connection_manager_(connection_manager),
      directory_(directory),
      update_handler_deleter_(&update_handler_map_),
      commit_contributor_deleter_(&commit_contributor_map_),
      extensions_activity_(extensions_activity),
      notifications_enabled_(false),
      max_commit_batch_size_(kDefaultMaxCommitBatchSize),
      debug_info_getter_(debug_info_getter),
      traffic_recorder_(traffic_recorder),
      keystore_encryption_enabled_(keystore_encryption_enabled),
      invalidator_client_id_(invalidator_client_id),
      server_enabled_pre_commit_update_avoidance_(false),
      client_enabled_pre_commit_update_avoidance_(
          client_enabled_pre_commit_update_avoidance) {
  for (size_t i = 0u; i < workers.size(); ++i) {
    workers_.insert(
        std::make_pair(workers[i]->GetModelSafeGroup(), workers[i]));
  }

  std::vector<SyncEngineEventListener*>::const_iterator it;
  for (it = listeners.begin(); it != listeners.end(); ++it)
    listeners_.AddObserver(*it);
}

SyncSessionContext::~SyncSessionContext() {
}

void SyncSessionContext::set_routing_info(
    const ModelSafeRoutingInfo& routing_info) {
  enabled_types_ = GetRoutingInfoTypes(routing_info);

  // TODO(rlarocque): This is not a good long-term solution.  We must find a
  // better way to initialize the set of CommitContributors and UpdateHandlers.
  STLDeleteValues<UpdateHandlerMap>(&update_handler_map_);
  STLDeleteValues<CommitContributorMap>(&commit_contributor_map_);
  for (ModelSafeRoutingInfo::const_iterator routing_iter = routing_info.begin();
       routing_iter != routing_info.end(); ++routing_iter) {
    ModelType type = routing_iter->first;
    ModelSafeGroup group = routing_iter->second;
    std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> >::iterator
        worker_it = workers_.find(group);
    DCHECK(worker_it != workers_.end());
    scoped_refptr<ModelSafeWorker> worker = worker_it->second;

    SyncDirectoryUpdateHandler* handler =
        new SyncDirectoryUpdateHandler(directory(), type, worker);
    update_handler_map_.insert(std::make_pair(type, handler));

    SyncDirectoryCommitContributor* contributor =
        new SyncDirectoryCommitContributor(directory(), type);
    commit_contributor_map_.insert(std::make_pair(type, contributor));
  }
}

}  // namespace sessions
}  // namespace syncer
