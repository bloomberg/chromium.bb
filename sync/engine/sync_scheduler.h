// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to schedule syncer tasks intelligently.
#ifndef SYNC_ENGINE_SYNC_SCHEDULER_H_
#define SYNC_ENGINE_SYNC_SCHEDULER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/engine/nudge_source.h"
#include "sync/internal_api/public/base/invalidation_interface.h"
#include "sync/sessions/sync_session.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

struct ServerConnectionEvent;

struct SYNC_EXPORT_PRIVATE ConfigurationParams {
  ConfigurationParams();
  ConfigurationParams(
      const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& source,
      ModelTypeSet types_to_download,
      const ModelSafeRoutingInfo& routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task);
  ~ConfigurationParams();

  // Source for the configuration.
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source;
  // The types that should be downloaded.
  ModelTypeSet types_to_download;
  // The new routing info (superset of types to be downloaded).
  ModelSafeRoutingInfo routing_info;
  // Callback to invoke on configuration completion.
  base::Closure ready_task;
  // Callback to invoke on configuration failure.
  base::Closure retry_task;
};

class SYNC_EXPORT_PRIVATE SyncScheduler
    : public sessions::SyncSession::Delegate {
 public:
  enum Mode {
    // In this mode, the thread only performs configuration tasks.  This is
    // designed to make the case where we want to download updates for a
    // specific type only, and not continue syncing until we are moved into
    // normal mode.
    CONFIGURATION_MODE,
    // Resumes polling and allows nudges, drops configuration tasks.  Runs
    // through entire sync cycle.
    NORMAL_MODE,
  };

  // All methods of SyncScheduler must be called on the same thread
  // (except for RequestEarlyExit()).

  SyncScheduler();
  virtual ~SyncScheduler();

  // Start the scheduler with the given mode.  If the scheduler is
  // already started, switch to the given mode, although some
  // scheduled tasks from the old mode may still run.
  virtual void Start(Mode mode) = 0;

  // Schedules the configuration task specified by |params|. Returns true if
  // the configuration task executed immediately, false if it had to be
  // scheduled for a later attempt. |params.ready_task| is invoked whenever the
  // configuration task executes. |params.retry_task| is invoked once if the
  // configuration task could not execute. |params.ready_task| will still be
  // called when configuration finishes.
  // Note: must already be in CONFIGURATION mode.
  virtual void ScheduleConfiguration(const ConfigurationParams& params) = 0;

  // Request that the syncer avoid starting any new tasks and prepare for
  // shutdown.
  virtual void Stop() = 0;

  // The meat and potatoes. All three of the following methods will post a
  // delayed task to attempt the actual nudge (see ScheduleNudgeImpl).
  //
  // NOTE: |desired_delay| is best-effort. If a nudge is already scheduled to
  // depart earlier than Now() + delay, the scheduler can and will prefer to
  // batch the two so that only one nudge is sent (at the earlier time). Also,
  // as always with delayed tasks and timers, it's possible the task gets run
  // any time after |desired_delay|.

  // The LocalNudge indicates that we've made a local change, and that the
  // syncer should plan to commit this to the server some time soon.
  virtual void ScheduleLocalNudge(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) = 0;

  // The LocalRefreshRequest occurs when we decide for some reason to manually
  // request updates.  This should be used sparingly.  For example, one of its
  // uses is to fetch the latest tab sync data when it's relevant to the UI on
  // platforms where tab sync is not registered for invalidations.
  virtual void ScheduleLocalRefreshRequest(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) = 0;

  // Invalidations are notifications the server sends to let us know when other
  // clients have committed data.  We need to contact the sync server (being
  // careful to pass along the "hints" delivered with those invalidations) in
  // order to fetch the update.
  virtual void ScheduleInvalidationNudge(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> invalidation,
      const tracked_objects::Location& nudge_location) = 0;

  // Requests a non-blocking initial sync request for the specified type.
  //
  // Many types can only complete initial sync while the scheduler is in
  // configure mode, but a few of them are able to perform their initial sync
  // while the scheduler is in normal mode.  This non-blocking initial sync
  // can be requested through this function.
  virtual void ScheduleInitialSyncNudge(syncer::ModelType model_type) = 0;

  // Change status of notifications in the SyncSessionContext.
  virtual void SetNotificationsEnabled(bool notifications_enabled) = 0;

  // Called when credentials are updated by the user.
  virtual void OnCredentialsUpdated() = 0;

  // Called when the network layer detects a connection status change.
  virtual void OnConnectionStatusChange() = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_SCHEDULER_H_
