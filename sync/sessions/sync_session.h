// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class representing an attempt to synchronize the local syncable data
// store with a sync server. A SyncSession instance is passed as a stateful
// bundle throughout the sync cycle.  The SyncSession is not reused across
// sync cycles; each cycle starts with a new one.

#ifndef SYNC_SESSIONS_SYNC_SESSION_H_
#define SYNC_SESSIONS_SYNC_SESSION_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/engine/sync_cycle_event.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/sessions/status_controller.h"
#include "sync/sessions/sync_session_context.h"

namespace syncer {
class ModelSafeWorker;
class ProtocolEvent;

namespace sessions {

class NudgeTracker;

class SYNC_EXPORT_PRIVATE SyncSession {
 public:
  // The Delegate services events that occur during the session requiring an
  // explicit (and session-global) action, as opposed to events that are simply
  // recorded in per-session state.
  class SYNC_EXPORT_PRIVATE Delegate {
   public:
    // The client was throttled and should cease-and-desist syncing activity
    // until the specified time.
    virtual void OnThrottled(const base::TimeDelta& throttle_duration) = 0;

    // Some of the client's types were throttled.
    virtual void OnTypesThrottled(
        ModelTypeSet types,
        const base::TimeDelta& throttle_duration) = 0;

    // Silenced intervals can be out of phase with individual sessions, so the
    // delegate is the only thing that can give an authoritative answer for
    // "is syncing silenced right now". This shouldn't be necessary very often
    // as the delegate ensures no session is started if syncing is silenced.
    // ** Note **  This will return true if silencing commenced during this
    // session and the interval has not yet elapsed, but the contract here is
    // solely based on absolute time values. So, this cannot be used to infer
    // that any given session _instance_ is silenced.  An example of reasonable
    // use is for UI reporting.
    virtual bool IsCurrentlyThrottled() = 0;

    // The client has been instructed to change its short poll interval.
    virtual void OnReceivedShortPollIntervalUpdate(
        const base::TimeDelta& new_interval) = 0;

    // The client has been instructed to change its long poll interval.
    virtual void OnReceivedLongPollIntervalUpdate(
        const base::TimeDelta& new_interval) = 0;

    // The client has been instructed to change a nudge delay.
    virtual void OnReceivedCustomNudgeDelays(
        const std::map<ModelType, base::TimeDelta>& nudge_delays) = 0;

    // Called for the syncer to respond to the error sent by the server.
    virtual void OnSyncProtocolError(
        const SyncProtocolError& sync_protocol_error) = 0;

    // Called when the server wants to change the number of hints the client
    // will buffer locally.
    virtual void OnReceivedClientInvalidationHintBufferSize(int size) = 0;

    // Called when server wants to schedule a retry GU.
    virtual void OnReceivedGuRetryDelay(const base::TimeDelta& delay) = 0;

    // Called when server requests a migration.
    virtual void OnReceivedMigrationRequest(ModelTypeSet types) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Build a session without a nudge tracker.  Used for poll or configure type
  // sync cycles.
  static SyncSession* Build(SyncSessionContext* context,
                            Delegate* delegate);
  ~SyncSession();

  // Builds a thread-safe and read-only copy of the current session state.
  SyncSessionSnapshot TakeSnapshot() const;
  SyncSessionSnapshot TakeSnapshotWithSource(
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource legacy_updates_source)
      const;

  // Builds and sends a snapshot to the session context's listeners.
  void SendSyncCycleEndEventNotification(
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source);
  void SendEventNotification(SyncCycleEvent::EventCause cause);

  void SendProtocolEvent(const ProtocolEvent& event);

  // TODO(akalin): Split this into context() and mutable_context().
  SyncSessionContext* context() const { return context_; }
  Delegate* delegate() const { return delegate_; }
  const StatusController& status_controller() const {
    return *status_controller_.get();
  }
  StatusController* mutable_status_controller() {
    return status_controller_.get();
  }

 private:
  SyncSession(SyncSessionContext* context, Delegate* delegate);

  // The context for this session, guaranteed to outlive |this|.
  SyncSessionContext* const context_;

  // The delegate for this session, must never be NULL.
  Delegate* const delegate_;

  // Our controller for various status and error counters.
  scoped_ptr<StatusController> status_controller_;

  DISALLOW_COPY_AND_ASSIGN(SyncSession);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_SYNC_SESSION_H_
