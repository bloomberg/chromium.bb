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
#include "sync/util/extensions_activity_monitor.h"

namespace syncer {
class ModelSafeWorker;

namespace syncable {
class WriteTransaction;
}

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
              const SyncSourceInfo& source,
              const ModelSafeRoutingInfo& routing_info,
              const std::vector<ModelSafeWorker*>& workers);
  ~SyncSession();

  // Builds a thread-safe and read-only copy of the current session state.
  SyncSessionSnapshot TakeSnapshot() const;

  // Builds and sends a snapshot to the session context's listeners.
  void SendEventNotification(SyncEngineEvent::EventCause cause);

  // Returns true if we reached the server. Note that "reaching the server"
  // here means that from an HTTP perspective, we succeeded (HTTP 200).  The
  // server **MAY** have returned a sync protocol error.
  // See SERVER_RETURN_* in the SyncerError enum for values.
  bool DidReachServer() const;

  // Collects all state pertaining to how and why |s| originated and unions it
  // with corresponding state in |this|, leaving |s| unchanged.  Allows |this|
  // to take on the responsibilities |s| had (e.g. certain data types) in the
  // next SyncShare operation using |this|, rather than needed two separate
  // sessions.
  void Coalesce(const SyncSession& session);

  // Compares the routing_info_, workers and payload map with those passed in.
  // Purges types from the above 3 which are not present in latest. Useful
  // to update the sync session when the user has disabled some types from
  // syncing.
  void RebaseRoutingInfoWithLatest(
      const ModelSafeRoutingInfo& routing_info,
      const std::vector<ModelSafeWorker*>& workers);

  // TODO(akalin): Split this into context() and mutable_context().
  SyncSessionContext* context() const { return context_; }
  Delegate* delegate() const { return delegate_; }
  syncable::WriteTransaction* write_transaction() { return write_transaction_; }
  const StatusController& status_controller() const {
    return *status_controller_.get();
  }
  StatusController* mutable_status_controller() {
    return status_controller_.get();
  }

  const ExtensionsActivityMonitor::Records& extensions_activity() const {
    return extensions_activity_;
  }
  ExtensionsActivityMonitor::Records* mutable_extensions_activity() {
    return &extensions_activity_;
  }

  const std::vector<ModelSafeWorker*>& workers() const { return workers_; }
  const ModelSafeRoutingInfo& routing_info() const { return routing_info_; }
  const SyncSourceInfo& source() const { return source_; }

  // Returns the set of groups which have enabled types.
  const std::set<ModelSafeGroup>& GetEnabledGroups() const;

 private:
  // Extend the encapsulation boundary to utilities for internal member
  // assignments. This way, the scope of these actions is explicit, they can't
  // be overridden, and assigning is always accompanied by unassigning.
  friend class ScopedSetSessionWriteTransaction;

  // The context for this session, guaranteed to outlive |this|.
  SyncSessionContext* const context_;

  // The source for initiating this sync session.
  SyncSourceInfo source_;

  // A list of sources for sessions that have been merged with this one.
  // Currently used only for logging.
  std::vector<SyncSourceInfo> debug_info_sources_list_;

  // Information about extensions activity since the last successful commit.
  ExtensionsActivityMonitor::Records extensions_activity_;

  // Used to allow various steps to share a transaction. Can be NULL.
  syncable::WriteTransaction* write_transaction_;

  // The delegate for this session, must never be NULL.
  Delegate* const delegate_;

  // Our controller for various status and error counters.
  scoped_ptr<StatusController> status_controller_;

  // The set of active ModelSafeWorkers for the duration of this session.
  // This can change if this session is Coalesce()'d with another.
  std::vector<ModelSafeWorker*> workers_;

  // The routing info for the duration of this session, dictating which
  // datatypes should be synced and which workers should be used when working
  // on those datatypes.
  ModelSafeRoutingInfo routing_info_;

  // The set of groups with enabled types.  Computed from
  // |routing_info_|.
  std::set<ModelSafeGroup> enabled_groups_;

  DISALLOW_COPY_AND_ASSIGN(SyncSession);
};

// Installs a WriteTransaction to a given session and later clears it when the
// utility falls out of scope. Transactions are not nestable, so it is an error
// to try and use one of these if the session already has a transaction.
class ScopedSetSessionWriteTransaction {
 public:
  ScopedSetSessionWriteTransaction(SyncSession* session,
                                   syncable::WriteTransaction* trans)
      : session_(session) {
    DCHECK(!session_->write_transaction_);
    session_->write_transaction_ = trans;
  }
  ~ScopedSetSessionWriteTransaction() { session_->write_transaction_ = NULL; }

 private:
  SyncSession* session_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSetSessionWriteTransaction);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_SYNC_SESSION_H_
