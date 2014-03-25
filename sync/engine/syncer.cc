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
#include "sync/engine/commit_processor.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/get_updates_delegate.h"
#include "sync/engine/get_updates_processor.h"
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
  if (nudge_tracker.IsGetUpdatesRequired() ||
      session->context()->ShouldFetchUpdatesBeforeCommit()) {
    VLOG(1) << "Downloading types " << ModelTypeSetToString(request_types);
    NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
    GetUpdatesProcessor get_updates_processor(
        session->context()->model_type_registry()->update_handler_map(),
        normal_delegate);
    if (!DownloadAndApplyUpdates(
            request_types,
            session,
            &get_updates_processor,
            kCreateMobileBookmarksFolder)) {
      return HandleCycleEnd(session, nudge_tracker.GetLegacySource());
    }
  }

  VLOG(1) << "Committing from types " << ModelTypeSetToString(request_types);
  CommitProcessor commit_processor(
      session->context()->model_type_registry()->commit_contributor_map());
  SyncerError commit_result =
      BuildAndPostCommits(request_types, session, &commit_processor);
  session->mutable_status_controller()->set_commit_result(commit_result);

  return HandleCycleEnd(session, nudge_tracker.GetLegacySource());
}

bool Syncer::ConfigureSyncShare(
    ModelTypeSet request_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    SyncSession* session) {
  VLOG(1) << "Configuring types " << ModelTypeSetToString(request_types);
  HandleCycleBegin(session);
  ConfigureGetUpdatesDelegate configure_delegate(source);
  GetUpdatesProcessor get_updates_processor(
      session->context()->model_type_registry()->update_handler_map(),
      configure_delegate);
  DownloadAndApplyUpdates(
      request_types,
      session,
      &get_updates_processor,
      kCreateMobileBookmarksFolder);
  return HandleCycleEnd(session, source);
}

bool Syncer::PollSyncShare(ModelTypeSet request_types,
                           SyncSession* session) {
  VLOG(1) << "Polling types " << ModelTypeSetToString(request_types);
  HandleCycleBegin(session);
  PollGetUpdatesDelegate poll_delegate;
  GetUpdatesProcessor get_updates_processor(
      session->context()->model_type_registry()->update_handler_map(),
      poll_delegate);
  DownloadAndApplyUpdates(
      request_types,
      session,
      &get_updates_processor,
      kCreateMobileBookmarksFolder);
  return HandleCycleEnd(session, sync_pb::GetUpdatesCallerInfo::PERIODIC);
}

bool Syncer::DownloadAndApplyUpdates(
    ModelTypeSet request_types,
    SyncSession* session,
    GetUpdatesProcessor* get_updates_processor,
    bool create_mobile_bookmarks_folder) {
  SyncerError download_result = UNSET;
  do {
    download_result = get_updates_processor->DownloadUpdates(
        request_types,
        session,
        create_mobile_bookmarks_folder);
  } while (download_result == SERVER_MORE_TO_DOWNLOAD);

  // Exit without applying if we're shutting down or an error was detected.
  if (download_result != SYNCER_OK)
    return false;
  if (ExitRequested())
    return false;

  {
    TRACE_EVENT0("sync", "ApplyUpdates");

    // Control type updates always get applied first.
    ApplyControlDataUpdates(session->context()->directory());

    // Apply upates to the other types.  May or may not involve cross-thread
    // traffic, depending on the underlying update handlers and the GU type's
    // delegate.
    get_updates_processor->ApplyUpdates(request_types,
                                        session->mutable_status_controller());

    session->context()->set_hierarchy_conflict_detected(
        session->status_controller().num_hierarchy_conflicts() > 0);
    session->SendEventNotification(SyncCycleEvent::STATUS_CHANGED);
  }

  if (ExitRequested())
    return false;
  return true;
}

SyncerError Syncer::BuildAndPostCommits(ModelTypeSet requested_types,
                                        sessions::SyncSession* session,
                                        CommitProcessor* commit_processor) {
  // The ExitRequested() check is unnecessary, since we should start getting
  // errors from the ServerConnectionManager if an exist has been requested.
  // However, it doesn't hurt to check it anyway.
  while (!ExitRequested()) {
    scoped_ptr<Commit> commit(
        Commit::Init(
            requested_types,
            session->context()->GetEnabledTypes(),
            session->context()->max_commit_batch_size(),
            session->context()->account_name(),
            session->context()->directory()->cache_guid(),
            commit_processor,
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
  session->SendEventNotification(SyncCycleEvent::SYNC_CYCLE_BEGIN);
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
