// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sync/engine/apply_control_data_updates.h"
#include "sync/engine/commit.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/download.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable-inl.h"

using base::Time;
using base::TimeDelta;
using sync_pb::ClientCommand;

namespace syncer {

// TODO(akalin): We may want to propagate this switch up
// eventually.
#if defined(OS_ANDROID) || defined(OS_IOS)
static const bool kCreateMobileBookmarksFolder = true;
#else
static const bool kCreateMobileBookmarksFolder = false;
#endif

using sessions::StatusController;
using sessions::SyncSession;
using sessions::NudgeTracker;

Syncer::Syncer(syncer::CancelationSignal* cancelation_signal)
    : cancelation_signal_(cancelation_signal) {
}

Syncer::~Syncer() {}

bool Syncer::ExitRequested() {
  return cancelation_signal_->IsSignalled();
}

bool Syncer::NormalSyncShare(ModelTypeSet request_types,
                             const NudgeTracker& nudge_tracker,
                             SyncSession* session) {
  HandleCycleBegin(session);
  VLOG(1) << "Downloading types " << ModelTypeSetToString(request_types);
  if (nudge_tracker.IsGetUpdatesRequired() ||
      session->context()->ShouldFetchUpdatesBeforeCommit()) {
    if (!DownloadAndApplyUpdates(
            request_types,
            session,
            base::Bind(&download::BuildNormalDownloadUpdates,
                       session,
                       kCreateMobileBookmarksFolder,
                       request_types,
                       base::ConstRef(nudge_tracker)))) {
      return HandleCycleEnd(session, nudge_tracker.updates_source());
    }
  }

  VLOG(1) << "Committing from types " << ModelTypeSetToString(request_types);
  SyncerError commit_result = BuildAndPostCommits(request_types, session);
  session->mutable_status_controller()->set_commit_result(commit_result);

  return HandleCycleEnd(session, nudge_tracker.updates_source());
}

bool Syncer::ConfigureSyncShare(
    ModelTypeSet request_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    SyncSession* session) {
  HandleCycleBegin(session);
  VLOG(1) << "Configuring types " << ModelTypeSetToString(request_types);
  DownloadAndApplyUpdates(
      request_types,
      session,
      base::Bind(&download::BuildDownloadUpdatesForConfigure,
                 session,
                 kCreateMobileBookmarksFolder,
                 source,
                 request_types));
  return HandleCycleEnd(session, source);
}

bool Syncer::PollSyncShare(ModelTypeSet request_types,
                           SyncSession* session) {
  HandleCycleBegin(session);
  VLOG(1) << "Polling types " << ModelTypeSetToString(request_types);
  DownloadAndApplyUpdates(
      request_types,
      session,
      base::Bind(&download::BuildDownloadUpdatesForPoll,
                 session,
                 kCreateMobileBookmarksFolder,
                 request_types));
  return HandleCycleEnd(session, sync_pb::GetUpdatesCallerInfo::PERIODIC);
}

void Syncer::ApplyUpdates(SyncSession* session) {
  TRACE_EVENT0("sync", "ApplyUpdates");

  ApplyControlDataUpdates(session->context()->directory());

  UpdateHandlerMap* handler_map = session->context()->update_handler_map();
  for (UpdateHandlerMap::iterator it = handler_map->begin();
       it != handler_map->end(); ++it) {
    it->second->ApplyUpdates(session->mutable_status_controller());
  }

  session->context()->set_hierarchy_conflict_detected(
      session->status_controller().num_hierarchy_conflicts() > 0);

  session->SendEventNotification(SyncEngineEvent::STATUS_CHANGED);
}

bool Syncer::DownloadAndApplyUpdates(
    ModelTypeSet request_types,
    SyncSession* session,
    base::Callback<void(sync_pb::ClientToServerMessage*)> build_fn) {
  SyncerError download_result = UNSET;
  do {
    TRACE_EVENT0("sync", "DownloadUpdates");
    sync_pb::ClientToServerMessage msg;
    build_fn.Run(&msg);
    download_result =
        download::ExecuteDownloadUpdates(request_types, session, &msg);
    session->mutable_status_controller()->set_last_download_updates_result(
        download_result);
  } while (download_result == SERVER_MORE_TO_DOWNLOAD);

  // Exit without applying if we're shutting down or an error was detected.
  if (download_result != SYNCER_OK)
    return false;
  if (ExitRequested())
    return false;

  ApplyUpdates(session);
  if (ExitRequested())
    return false;
  return true;
}

SyncerError Syncer::BuildAndPostCommits(ModelTypeSet requested_types,
                                        sessions::SyncSession* session) {
  // The ExitRequested() check is unnecessary, since we should start getting
  // errors from the ServerConnectionManager if an exist has been requested.
  // However, it doesn't hurt to check it anyway.
  while (!ExitRequested()) {
    scoped_ptr<Commit> commit(
        Commit::Init(
            requested_types,
            session->context()->max_commit_batch_size(),
            session->context()->account_name(),
            session->context()->directory()->cache_guid(),
            session->context()->commit_contributor_map(),
            session->context()->extensions_activity()));
    if (!commit) {
      break;
    }

    SyncerError error = commit->PostAndProcessResponse(
        session,
        session->mutable_status_controller(),
        session->context()->extensions_activity());
    commit->CleanUp();
    if (error != SYNCER_OK) {
      return error;
    }
  }

  return SYNCER_OK;
}

void Syncer::HandleCycleBegin(SyncSession* session) {
  session->mutable_status_controller()->UpdateStartTime();
  session->SendEventNotification(SyncEngineEvent::SYNC_CYCLE_BEGIN);
}

bool Syncer::HandleCycleEnd(
    SyncSession* session,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  if (!ExitRequested()) {
    session->SendSyncCycleEndEventNotification(source);
    return true;
  } else {
    return false;
  }
}

}  // namespace syncer
