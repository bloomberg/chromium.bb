// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sync/engine/apply_control_data_updates.h"
#include "sync/engine/apply_updates_and_resolve_conflicts_command.h"
#include "sync/engine/build_commit_command.h"
#include "sync/engine/commit.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/download.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/process_commit_response_command.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/sessions/nudge_tracker.h"
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

Syncer::Syncer()
    : early_exit_requested_(false) {
}

Syncer::~Syncer() {}

bool Syncer::ExitRequested() {
  base::AutoLock lock(early_exit_requested_lock_);
  return early_exit_requested_;
}

void Syncer::RequestEarlyExit() {
  base::AutoLock lock(early_exit_requested_lock_);
  early_exit_requested_ = true;
}

bool Syncer::NormalSyncShare(ModelTypeSet request_types,
                             const NudgeTracker& nudge_tracker,
                             SyncSession* session) {
  HandleCycleBegin(session);
  VLOG(1) << "Downloading types " << ModelTypeSetToString(request_types);
  if (!DownloadAndApplyUpdates(
          session,
          base::Bind(&NormalDownloadUpdates,
                     session,
                     kCreateMobileBookmarksFolder,
                     request_types,
                     base::ConstRef(nudge_tracker)))) {
    return HandleCycleEnd(session);
  }

  VLOG(1) << "Committing from types " << ModelTypeSetToString(request_types);
  SyncerError commit_result = BuildAndPostCommits(request_types, this, session);
  session->mutable_status_controller()->set_commit_result(commit_result);

  return HandleCycleEnd(session);
}

bool Syncer::ConfigureSyncShare(ModelTypeSet request_types,
                                SyncSession* session) {
  HandleCycleBegin(session);
  VLOG(1) << "Configuring types " << ModelTypeSetToString(request_types);
  DownloadAndApplyUpdates(
      session,
      base::Bind(&DownloadUpdatesForConfigure,
                 session,
                 kCreateMobileBookmarksFolder,
                 session->source(),
                 request_types));
  return HandleCycleEnd(session);
}

bool Syncer::PollSyncShare(ModelTypeSet request_types,
                           SyncSession* session) {
  HandleCycleBegin(session);
  VLOG(1) << "Polling types " << ModelTypeSetToString(request_types);
  DownloadAndApplyUpdates(
      session,
      base::Bind(&DownloadUpdatesForPoll,
                 session,
                 kCreateMobileBookmarksFolder,
                 request_types));
  return HandleCycleEnd(session);
}

void Syncer::ApplyUpdates(SyncSession* session) {
  TRACE_EVENT0("sync", "ApplyUpdates");

  ApplyControlDataUpdates(session);

  ApplyUpdatesAndResolveConflictsCommand apply_updates;
  apply_updates.Execute(session);

  session->context()->set_hierarchy_conflict_detected(
      session->status_controller().num_hierarchy_conflicts() > 0);

  session->SendEventNotification(SyncEngineEvent::STATUS_CHANGED);
}

bool Syncer::DownloadAndApplyUpdates(
    SyncSession* session,
    base::Callback<SyncerError(void)> download_fn) {
  while (!session->status_controller().ServerSaysNothingMoreToDownload()) {
    SyncerError download_result = download_fn.Run();
    session->mutable_status_controller()->set_last_download_updates_result(
        download_result);
    if (download_result != SYNCER_OK) {
      return false;
    }
  }
  if (ExitRequested())
    return false;
  ApplyUpdates(session);
  if (ExitRequested())
    return false;
  return true;
}

void Syncer::HandleCycleBegin(SyncSession* session) {
  session->mutable_status_controller()->UpdateStartTime();
  session->SendEventNotification(SyncEngineEvent::SYNC_CYCLE_BEGIN);
}

bool Syncer::HandleCycleEnd(SyncSession* session) {
  if (!ExitRequested()) {
    session->SendEventNotification(SyncEngineEvent::SYNC_CYCLE_ENDED);
    return true;
  } else {
    return false;
  }
}

}  // namespace syncer
