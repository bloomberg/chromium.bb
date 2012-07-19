// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to schedule syncer tasks intelligently.
#ifndef SYNC_ENGINE_SYNC_SCHEDULER_H_
#define SYNC_ENGINE_SYNC_SCHEDULER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "sync/engine/nudge_source.h"
#include "sync/internal_api/public/base/model_type_payload_map.h"
#include "sync/sessions/sync_session.h"

class MessageLoop;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

struct ServerConnectionEvent;

struct ConfigurationParams {
  enum KeystoreKeyStatus {
    KEYSTORE_KEY_UNNECESSARY,
    KEYSTORE_KEY_NEEDED
  };
  ConfigurationParams();
  ConfigurationParams(
      const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& source,
      const syncer::ModelTypeSet& types_to_download,
      const syncer::ModelSafeRoutingInfo& routing_info,
      KeystoreKeyStatus keystore_key_status,
      const base::Closure& ready_task);
  ~ConfigurationParams();

  // Source for the configuration.
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source;
  // The types that should be downloaded.
  syncer::ModelTypeSet types_to_download;
  // The new routing info (superset of types to be downloaded).
  ModelSafeRoutingInfo routing_info;
  // Whether we need to perform a GetKey command.
  KeystoreKeyStatus keystore_key_status;
  // Callback to invoke on configuration completion.
  base::Closure ready_task;
};

class SyncScheduler : public sessions::SyncSession::Delegate {
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
  // configuration task executes.
  // Note: must already be in CONFIGURATION mode.
  virtual bool ScheduleConfiguration(const ConfigurationParams& params) = 0;

  // Request that any running syncer task stop as soon as possible and
  // cancel all scheduled tasks. This function can be called from any thread,
  // and should in fact be called from a thread that isn't the sync loop to
  // allow preempting ongoing sync cycles.
  // Invokes |callback| from the sync loop once syncer is idle and all tasks
  // are cancelled.
  virtual void RequestStop(const base::Closure& callback) = 0;

  // The meat and potatoes. Both of these methods will post a delayed task
  // to attempt the actual nudge (see ScheduleNudgeImpl).
  virtual void ScheduleNudgeAsync(
      const base::TimeDelta& delay,
      NudgeSource source,
      syncer::ModelTypeSet types,
      const tracked_objects::Location& nudge_location) = 0;
  virtual void ScheduleNudgeWithPayloadsAsync(
      const base::TimeDelta& delay, NudgeSource source,
      const syncer::ModelTypePayloadMap& types_with_payloads,
      const tracked_objects::Location& nudge_location) = 0;

  // Change status of notifications in the SyncSessionContext.
  virtual void SetNotificationsEnabled(bool notifications_enabled) = 0;

  virtual base::TimeDelta GetSessionsCommitDelay() const = 0;

  // Called when credentials are updated by the user.
  virtual void OnCredentialsUpdated() = 0;

  // Called when the network layer detects a connection status change.
  virtual void OnConnectionStatusChange() = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_SCHEDULER_H_
