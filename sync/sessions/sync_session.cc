// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/syncable/directory.h"

namespace syncer {
namespace sessions {

namespace {

std::set<ModelSafeGroup> ComputeEnabledGroups(
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers) {
  std::set<ModelSafeGroup> enabled_groups;
  // Project the list of enabled types (i.e., types in the routing
  // info) to a list of enabled groups.
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    enabled_groups.insert(it->second);
  }
  // GROUP_PASSIVE is always enabled, since that's the group that
  // top-level folders map to.
  enabled_groups.insert(GROUP_PASSIVE);
  if (DCHECK_IS_ON()) {
    // We sometimes create dummy SyncSession objects (see
    // SyncScheduler::InitialSnapshot) so don't check in that case.
    if (!routing_info.empty() || !workers.empty()) {
      std::set<ModelSafeGroup> groups_with_workers;
      for (std::vector<ModelSafeWorker*>::const_iterator it = workers.begin();
           it != workers.end(); ++it) {
        groups_with_workers.insert((*it)->GetModelSafeGroup());
      }
      // All enabled groups should have a corresponding worker.
      DCHECK(std::includes(
          groups_with_workers.begin(), groups_with_workers.end(),
          enabled_groups.begin(), enabled_groups.end()));
    }
  }
  return enabled_groups;
}

void PurgeStaleStates(ModelTypeInvalidationMap* original,
                      const ModelSafeRoutingInfo& routing_info) {
  std::vector<ModelTypeInvalidationMap::iterator> iterators_to_delete;
  for (ModelTypeInvalidationMap::iterator i = original->begin();
       i != original->end(); ++i) {
    if (routing_info.end() == routing_info.find(i->first)) {
      iterators_to_delete.push_back(i);
    }
  }

  for (std::vector<ModelTypeInvalidationMap::iterator>::iterator
       it = iterators_to_delete.begin(); it != iterators_to_delete.end();
       ++it) {
    original->erase(*it);
  }
}

}  // namesepace

SyncSession::SyncSession(SyncSessionContext* context, Delegate* delegate,
    const SyncSourceInfo& source,
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers)
    : context_(context),
      source_(source),
      write_transaction_(NULL),
      delegate_(delegate),
      workers_(workers),
      routing_info_(routing_info),
      enabled_groups_(ComputeEnabledGroups(routing_info_, workers_)) {
  status_controller_.reset(new StatusController(routing_info_));
  std::sort(workers_.begin(), workers_.end());
  debug_info_sources_list_.push_back(source_);
}

SyncSession::~SyncSession() {}

void SyncSession::Coalesce(const SyncSession& session) {
  if (context_ != session.context() || delegate_ != session.delegate_) {
    NOTREACHED();
    return;
  }

  // When we coalesce sessions, the sync update source gets overwritten with the
  // most recent, while the type/state map gets merged.
  debug_info_sources_list_.push_back(session.source_);
  CoalesceStates(&source_.types, session.source_.types);
  source_.updates_source = session.source_.updates_source;

  std::vector<ModelSafeWorker*> temp;
  std::set_union(workers_.begin(), workers_.end(),
                 session.workers_.begin(), session.workers_.end(),
                 std::back_inserter(temp));
  workers_.swap(temp);

  // We have to update the model safe routing info to the union. In case the
  // same key is present in both pick the one from session.
  for (ModelSafeRoutingInfo::const_iterator it =
       session.routing_info_.begin();
       it != session.routing_info_.end();
       ++it) {
    routing_info_[it->first] = it->second;
  }

  // Now update enabled groups.
  enabled_groups_ = ComputeEnabledGroups(routing_info_, workers_);
}

void SyncSession::RebaseRoutingInfoWithLatest(
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers) {
  ModelSafeRoutingInfo temp_routing_info;

  // Take the intersection and also set the routing info(it->second) from the
  // passed in session.
  for (ModelSafeRoutingInfo::const_iterator it =
       routing_info.begin(); it != routing_info.end();
       ++it) {
    if (routing_info_.find(it->first) != routing_info_.end()) {
      temp_routing_info[it->first] = it->second;
    }
  }
  routing_info_.swap(temp_routing_info);

  PurgeStaleStates(&source_.types, routing_info);

  // Now update the workers.
  std::vector<ModelSafeWorker*> temp;
  std::vector<ModelSafeWorker*> sorted_workers = workers;
  std::sort(sorted_workers.begin(), sorted_workers.end());
  std::set_intersection(workers_.begin(), workers_.end(),
                        sorted_workers.begin(), sorted_workers.end(),
                        std::back_inserter(temp));
  workers_.swap(temp);

  // Now update enabled groups.
  enabled_groups_ = ComputeEnabledGroups(routing_info_, workers_);
}

SyncSessionSnapshot SyncSession::TakeSnapshot() const {
  syncable::Directory* dir = context_->directory();

  bool is_share_useable = true;
  ModelTypeSet initial_sync_ended;
  ProgressMarkerMap download_progress_markers;
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    ModelType type(ModelTypeFromInt(i));
    if (routing_info_.count(type) != 0) {
      if (dir->initial_sync_ended_for_type(type))
        initial_sync_ended.Put(type);
      else
        is_share_useable = false;
    }
    dir->GetDownloadProgressAsString(type, &download_progress_markers[type]);
  }

  std::vector<int> num_entries_by_type(MODEL_TYPE_COUNT, 0);
  std::vector<int> num_to_delete_entries_by_type(MODEL_TYPE_COUNT, 0);
  dir->CollectMetaHandleCounts(&num_entries_by_type,
                               &num_to_delete_entries_by_type);

  SyncSessionSnapshot snapshot(
      status_controller_->model_neutral_state(),
      is_share_useable,
      initial_sync_ended,
      download_progress_markers,
      delegate_->IsSyncingCurrentlySilenced(),
      status_controller_->num_encryption_conflicts(),
      status_controller_->num_hierarchy_conflicts(),
      status_controller_->num_server_conflicts(),
      source_,
      debug_info_sources_list_,
      context_->notifications_enabled(),
      dir->GetEntriesCount(),
      status_controller_->sync_start_time(),
      num_entries_by_type,
      num_to_delete_entries_by_type);

  return snapshot;
}

void SyncSession::SendEventNotification(SyncEngineEvent::EventCause cause) {
  SyncEngineEvent event(cause);
  event.snapshot = TakeSnapshot();

  DVLOG(1) << "Sending event with snapshot: " << event.snapshot.ToString();
  context()->NotifyListeners(event);
}

const std::set<ModelSafeGroup>& SyncSession::GetEnabledGroups() const {
  return enabled_groups_;
}

bool SyncSession::DidReachServer() const {
  const ModelNeutralState& state = status_controller_->model_neutral_state();
  return state.last_get_key_result >= FIRST_SERVER_RETURN_VALUE ||
      state.last_download_updates_result >= FIRST_SERVER_RETURN_VALUE ||
      state.commit_result >= FIRST_SERVER_RETURN_VALUE;
}

}  // namespace sessions
}  // namespace syncer
