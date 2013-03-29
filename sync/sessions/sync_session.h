// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class representing an attempt to synchronize the local syncable data
// store with a sync server. A SyncSession instance is passed as a stateful
// bundle to and from various SyncerCommands with the goal of converging the
// client view of data with that of the server. The commands twiddle with
// session status in response to events and hiccups along the way, set and
// query session progress with regards to conflict resolution and applying
// server updates, and access the SyncSessionContext for the current session
// via SyncSession instances.

#ifndef SYNC_SESSIONS_SYNC_SESSION_H_
#define SYNC_SESSIONS_SYNC_SESSION_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/sessions/ordered_commit_set.h"
#include "sync/sessions/status_controller.h"
#include "sync/sessions/sync_session_context.h"

namespace syncer {
class ModelSafeWorker;

namespace sessions {

class SYNC_EXPORT_PRIVATE SyncSession {
 public:
  // The Delegate services events that occur during the session requiring an
  // explicit (and session-global) action, as opposed to events that are simply
  // recorded in per-session state.
  class SYNC_EXPORT_PRIVATE Delegate {
   public:
    // The client was throttled and should cease-and-desist syncing activity
    // until the specified time.
    virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) = 0;

    // Silenced intervals can be out of phase with individual sessions, so the
    // delegate is the only thing that can give an authoritative answer for
    // "is syncing silenced right now". This shouldn't be necessary very often
    // as the delegate ensures no session is started if syncing is silenced.
    // ** Note **  This will return true if silencing commenced during this
    // session and the interval has not yet elapsed, but the contract here is
    // solely based on absolute time values. So, this cannot be used to infer
    // that any given session _instance_ is silenced.  An example of reasonable
    // use is for UI reporting.
    virtual bool IsSyncingCurrentlySilenced() = 0;

    // The client has been instructed to change its short poll interval.
    virtual void OnReceivedShortPollIntervalUpdate(
        const base::TimeDelta& new_interval) = 0;

    // The client has been instructed to change its long poll interval.
    virtual void OnReceivedLongPollIntervalUpdate(
        const base::TimeDelta& new_interval) = 0;

    // The client has been instructed to change its sessions commit
    // delay.
    virtual void OnReceivedSessionsCommitDelay(
        const base::TimeDelta& new_delay) = 0;

    // The client needs to cease and desist syncing at once.  This occurs when
    // the Syncer detects that the backend store has fundamentally changed or
    // is a different instance altogether (e.g. swapping from a test instance
    // to production, or a global stop syncing operation has wiped the store).
    // TODO(lipalani) : Replace this function with the one below. This function
    // stops the current sync cycle and purges the client. In the new model
    // the former would be done by the |SyncProtocolError| and
    // the latter(which is an action) would be done in ProfileSyncService
    // along with the rest of the actions.
    virtual void OnShouldStopSyncingPermanently() = 0;

    // Called for the syncer to respond to the error sent by the server.
    virtual void OnSyncProtocolError(
        const sessions::SyncSessionSnapshot& snapshot) = 0;

   protected:
    virtual ~Delegate() {}
  };

  SyncSession(SyncSessionContext* context,
              Delegate* delegate,
              const SyncSourceInfo& source);
  ~SyncSession();

  // Builds a thread-safe and read-only copy of the current session state.
  SyncSessionSnapshot TakeSnapshot() const;

  // Builds and sends a snapshot to the session context's listeners.
  void SendEventNotification(SyncEngineEvent::EventCause cause);

  // TODO(akalin): Split this into context() and mutable_context().
  SyncSessionContext* context() const { return context_; }
  Delegate* delegate() const { return delegate_; }
  const StatusController& status_controller() const {
    return *status_controller_.get();
  }
  StatusController* mutable_status_controller() {
    return status_controller_.get();
  }

  const SyncSourceInfo& source() const { return source_; }

 private:
  // The context for this session, guaranteed to outlive |this|.
  SyncSessionContext* const context_;

  // The source for initiating this sync session.
  SyncSourceInfo source_;

  // The delegate for this session, must never be NULL.
  Delegate* const delegate_;

  // Our controller for various status and error counters.
  scoped_ptr<StatusController> status_controller_;

  DISALLOW_COPY_AND_ASSIGN(SyncSession);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_SYNC_SESSION_H_
