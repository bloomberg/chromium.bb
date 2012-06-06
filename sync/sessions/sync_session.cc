// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "sync/syncable/model_type.h"
#include "sync/syncable/syncable.h"

namespace browser_sync {
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
      enabled_groups_(ComputeEnabledGroups(routing_info_, workers_)),
      finished_(false) {
  status_controller_.reset(new StatusController(routing_info_));
  std::sort(workers_.begin(), workers_.end());
}

SyncSession::~SyncSession() {}

void SyncSession::Coalesce(const SyncSession& session) {
  if (context_ != session.context() || delegate_ != session.delegate_) {
    NOTREACHED();
    return;
  }

  // When we coalesce sessions, the sync update source gets overwritten with the
  // most recent, while the type/payload map gets merged.
  CoalescePayloads(&source_.types, session.source_.types);
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

void SyncSession::RebaseRoutingInfoWithLatest(const SyncSession& session) {
  ModelSafeRoutingInfo temp_routing_info;

  // Take the intersecion and also set the routing info(it->second) from the
  // passed in session.
  for (ModelSafeRoutingInfo::const_iterator it =
       session.routing_info_.begin(); it != session.routing_info_.end();
       ++it) {
    if (routing_info_.find(it->first) != routing_info_.end()) {
      temp_routing_info[it->first] = it->second;
    }
  }

  // Now swap it.
  routing_info_.swap(temp_routing_info);

  // Now update the payload map.
  PurgeStalePayload(&source_.types, session.routing_info_);

  // Now update the workers.
  std::vector<ModelSafeWorker*> temp;
  std::set_intersection(workers_.begin(), workers_.end(),
                 session.workers_.begin(), session.workers_.end(),
                 std::back_inserter(temp));
  workers_.swap(temp);

  // Now update enabled groups.
  enabled_groups_ = ComputeEnabledGroups(routing_info_, workers_);
}

void SyncSession::PrepareForAnotherSyncCycle() {
  finished_ = false;
  source_.updates_source =
      sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION;
  status_controller_.reset(new StatusController(routing_info_));
}

SyncSessionSnapshot SyncSession::TakeSnapshot() const {
  syncable::Directory* dir = context_->directory();

  bool is_share_useable = true;
  syncable::ModelTypeSet initial_sync_ended;
  syncable::ModelTypePayloadMap download_progress_markers;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType type(syncable::ModelTypeFromInt(i));
    if (routing_info_.count(type) != 0) {
      if (dir->initial_sync_ended_for_type(type))
        initial_sync_ended.Put(type);
      else
        is_share_useable = false;
    }
    dir->GetDownloadProgressAsString(type, &download_progress_markers[type]);
  }

  return SyncSessionSnapshot(
      status_controller_->syncer_status(),
      status_controller_->error(),
      status_controller_->num_server_changes_remaining(),
      is_share_useable,
      initial_sync_ended,
      download_progress_markers,
      HasMoreToSync(),
      delegate_->IsSyncingCurrentlySilenced(),
      status_controller_->TotalNumEncryptionConflictingItems(),
      status_controller_->TotalNumHierarchyConflictingItems(),
      status_controller_->TotalNumSimpleConflictingItems(),
      status_controller_->TotalNumServerConflictingItems(),
      source_,
      context_->notifications_enabled(),
      dir->GetEntriesCount(),
      status_controller_->sync_start_time(),
      !Succeeded());
}

void SyncSession::SendEventNotification(SyncEngineEvent::EventCause cause) {
  SyncEngineEvent event(cause);
  event.snapshot = TakeSnapshot();

  DVLOG(1) << "Sending event with snapshot: " << event.snapshot.ToString();
  context()->NotifyListeners(event);
}

bool SyncSession::HasMoreToSync() const {
  const StatusController* status = status_controller_.get();
  return status->conflicts_resolved();
}

const std::set<ModelSafeGroup>& SyncSession::GetEnabledGroups() const {
  return enabled_groups_;
}

std::set<ModelSafeGroup> SyncSession::GetEnabledGroupsWithConflicts() const {
  const std::set<ModelSafeGroup>& enabled_groups = GetEnabledGroups();
  std::set<ModelSafeGroup> enabled_groups_with_conflicts;
  for (std::set<ModelSafeGroup>::const_iterator it =
           enabled_groups.begin(); it != enabled_groups.end(); ++it) {
    const sessions::ConflictProgress* conflict_progress =
        status_controller_->GetUnrestrictedConflictProgress(*it);
    if (conflict_progress &&
        (conflict_progress->SimpleConflictingItemsBegin() !=
         conflict_progress->SimpleConflictingItemsEnd())) {
      enabled_groups_with_conflicts.insert(*it);
    }
  }
  return enabled_groups_with_conflicts;
}

std::set<ModelSafeGroup>
    SyncSession::GetEnabledGroupsWithVerifiedUpdates() const {
  const std::set<ModelSafeGroup>& enabled_groups = GetEnabledGroups();
  std::set<ModelSafeGroup> enabled_groups_with_verified_updates;
  for (std::set<ModelSafeGroup>::const_iterator it =
           enabled_groups.begin(); it != enabled_groups.end(); ++it) {
    const UpdateProgress* update_progress =
        status_controller_->GetUnrestrictedUpdateProgress(*it);
    if (update_progress &&
        (update_progress->VerifiedUpdatesBegin() !=
         update_progress->VerifiedUpdatesEnd())) {
      enabled_groups_with_verified_updates.insert(*it);
    }
  }

  return enabled_groups_with_verified_updates;
}

namespace {
// Return true if the command in question was attempted and did not complete
// successfully.
//
bool IsError(SyncerError error) {
  return error != UNSET && error != SYNCER_OK;
}

// Returns false iff one of the command results had an error.
bool HadErrors(const ErrorCounters& error) {
  const bool download_updates_error =
      IsError(error.last_download_updates_result);
  const bool post_commit_error = IsError(error.last_post_commit_result);
  const bool process_commit_response_error =
      IsError(error.last_process_commit_response_result);
  return download_updates_error ||
         post_commit_error ||
         process_commit_response_error;
}
}  // namespace

bool SyncSession::Succeeded() const {
  const ErrorCounters& error = status_controller_->error();
  return finished_ && !HadErrors(error);
}

bool SyncSession::SuccessfullyReachedServer() const {
  const ErrorCounters& error = status_controller_->error();
  bool reached_server = error.last_download_updates_result == SYNCER_OK ||
                        error.last_post_commit_result == SYNCER_OK ||
                        error.last_process_commit_response_result == SYNCER_OK;
  // It's possible that we reached the server on one attempt, then had an error
  // on the next (or didn't perform some of the server-communicating commands).
  // We want to verify that, for all commands attempted, we successfully spoke
  // with the server. Therefore, we verify no errors and at least one SYNCER_OK.
  return reached_server && !HadErrors(error);
}

void SyncSession::SetFinished() {
  finished_ = true;
}

}  // namespace sessions
}  // namespace browser_sync
