// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "build/build_config.h"
#include "sync/engine/apply_control_data_updates.h"
#include "sync/engine/apply_updates_and_resolve_conflicts_command.h"
#include "sync/engine/build_commit_command.h"
#include "sync/engine/commit.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/download_updates_command.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/process_commit_response_command.h"
#include "sync/engine/process_updates_command.h"
#include "sync/engine/store_timestamps_command.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable-inl.h"

using base::Time;
using base::TimeDelta;
using sync_pb::ClientCommand;

namespace syncer {

using sessions::StatusController;
using sessions::SyncSession;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::SERVER_CTIME;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_MTIME;
using syncable::SERVER_NON_UNIQUE_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_SPECIFICS;
using syncable::SERVER_UNIQUE_POSITION;
using syncable::SERVER_VERSION;

#define ENUM_CASE(x) case x: return #x
const char* SyncerStepToString(const SyncerStep step)
{
  switch (step) {
    ENUM_CASE(SYNCER_BEGIN);
    ENUM_CASE(DOWNLOAD_UPDATES);
    ENUM_CASE(PROCESS_UPDATES);
    ENUM_CASE(STORE_TIMESTAMPS);
    ENUM_CASE(APPLY_UPDATES);
    ENUM_CASE(COMMIT);
    ENUM_CASE(SYNCER_END);
  }
  NOTREACHED();
  return "";
}
#undef ENUM_CASE

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

bool Syncer::SyncShare(sessions::SyncSession* session,
                       SyncerStep first_step,
                       SyncerStep last_step) {
  session->mutable_status_controller()->UpdateStartTime();
  SyncerStep current_step = first_step;

  SyncerStep next_step = current_step;
  while (!ExitRequested()) {
    TRACE_EVENT1("sync", "SyncerStateMachine",
                 "state", SyncerStepToString(current_step));
    DVLOG(1) << "Syncer step:" << SyncerStepToString(current_step);

    switch (current_step) {
      case SYNCER_BEGIN:
        session->context()->throttled_data_type_tracker()->
            PruneUnthrottledTypes(base::TimeTicks::Now());
        session->SendEventNotification(SyncEngineEvent::SYNC_CYCLE_BEGIN);

        next_step = DOWNLOAD_UPDATES;
        break;
      case DOWNLOAD_UPDATES: {
        // TODO(akalin): We may want to propagate this switch up
        // eventually.
#if defined(OS_ANDROID) || defined(OS_IOS)
        const bool kCreateMobileBookmarksFolder = true;
#else
        const bool kCreateMobileBookmarksFolder = false;
#endif
        DownloadUpdatesCommand download_updates(kCreateMobileBookmarksFolder);
        session->mutable_status_controller()->set_last_download_updates_result(
            download_updates.Execute(session));
        next_step = PROCESS_UPDATES;
        break;
      }
      case PROCESS_UPDATES: {
        ProcessUpdatesCommand process_updates;
        process_updates.Execute(session);
        next_step = STORE_TIMESTAMPS;
        break;
      }
      case STORE_TIMESTAMPS: {
        StoreTimestampsCommand store_timestamps;
        store_timestamps.Execute(session);
        session->SendEventNotification(SyncEngineEvent::STATUS_CHANGED);
        // We download all of the updates before attempting to apply them.
        if (!session->status_controller().download_updates_succeeded()) {
          // We may have downloaded some updates, but if the latest download
          // attempt failed then we don't have all the updates.  We'll leave
          // it to a retry job to pick up where we left off.
          last_step = SYNCER_END; // Necessary for CONFIGURATION mode.
          next_step = SYNCER_END;
          DVLOG(1) << "Aborting sync cycle due to download updates failure";
        } else if (!session->status_controller()
                       .ServerSaysNothingMoreToDownload()) {
          next_step = DOWNLOAD_UPDATES;
        } else {
          next_step = APPLY_UPDATES;
        }
        break;
      }
      case APPLY_UPDATES: {
        // These include encryption updates that should be applied early.
        ApplyControlDataUpdates(session);

        ApplyUpdatesAndResolveConflictsCommand apply_updates;
        apply_updates.Execute(session);

        session->context()->set_hierarchy_conflict_detected(
            session->status_controller().num_hierarchy_conflicts() > 0);

        session->SendEventNotification(SyncEngineEvent::STATUS_CHANGED);
        if (last_step == APPLY_UPDATES) {
          // We're in configuration mode, but we still need to run the
          // SYNCER_END step.
          last_step = SYNCER_END;
          next_step = SYNCER_END;
        } else {
          next_step = COMMIT;
        }
        break;
      }
      case COMMIT: {
        session->mutable_status_controller()->set_commit_result(
            BuildAndPostCommits(this, session));
        next_step = SYNCER_END;
        break;
      }
      case SYNCER_END: {
        session->SendEventNotification(SyncEngineEvent::SYNC_CYCLE_ENDED);
        next_step = SYNCER_END;
        break;
      }
      default:
        LOG(ERROR) << "Unknown command: " << current_step;
    }  // switch
    DVLOG(2) << "last step: " << SyncerStepToString(last_step) << ", "
             << "current step: " << SyncerStepToString(current_step) << ", "
             << "next step: " << SyncerStepToString(next_step) << ", "
             << "snapshot: " << session->TakeSnapshot().ToString();
    if (last_step == current_step)
      return true;
    current_step = next_step;
  }  // while
  return false;
}

void CopyServerFields(syncable::Entry* src, syncable::MutableEntry* dest) {
  dest->Put(SERVER_NON_UNIQUE_NAME, src->Get(SERVER_NON_UNIQUE_NAME));
  dest->Put(SERVER_PARENT_ID, src->Get(SERVER_PARENT_ID));
  dest->Put(SERVER_MTIME, src->Get(SERVER_MTIME));
  dest->Put(SERVER_CTIME, src->Get(SERVER_CTIME));
  dest->Put(SERVER_VERSION, src->Get(SERVER_VERSION));
  dest->Put(SERVER_IS_DIR, src->Get(SERVER_IS_DIR));
  dest->Put(SERVER_IS_DEL, src->Get(SERVER_IS_DEL));
  dest->Put(IS_UNAPPLIED_UPDATE, src->Get(IS_UNAPPLIED_UPDATE));
  dest->Put(SERVER_SPECIFICS, src->Get(SERVER_SPECIFICS));
  dest->Put(SERVER_UNIQUE_POSITION, src->Get(SERVER_UNIQUE_POSITION));
}

}  // namespace syncer
