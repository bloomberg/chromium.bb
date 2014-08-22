// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_
#define SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "sync/base/sync_export.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/nudge_source.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/sessions/sync_session.h"
#include "sync/sessions/sync_session_context.h"

namespace syncer {

class BackoffDelayProvider;

namespace sessions {
struct ModelNeutralState;
}

class SYNC_EXPORT_PRIVATE SyncSchedulerImpl
    : public SyncScheduler,
      public base::NonThreadSafe {
 public:
  // |name| is a display string to identify the syncer thread.  Takes
  // |ownership of |syncer| and |delay_provider|.
  SyncSchedulerImpl(const std::string& name,
                    BackoffDelayProvider* delay_provider,
                    sessions::SyncSessionContext* context,
                    Syncer* syncer);

  // Calls Stop().
  virtual ~SyncSchedulerImpl();

  virtual void Start(Mode mode) OVERRIDE;
  virtual void ScheduleConfiguration(
      const ConfigurationParams& params) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void ScheduleLocalNudge(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void ScheduleLocalRefreshRequest(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void ScheduleInvalidationNudge(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> invalidation,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void ScheduleInitialSyncNudge(syncer::ModelType model_type) OVERRIDE;
  virtual void SetNotificationsEnabled(bool notifications_enabled) OVERRIDE;

  virtual void OnCredentialsUpdated() OVERRIDE;
  virtual void OnConnectionStatusChange() OVERRIDE;

  // SyncSession::Delegate implementation.
  virtual void OnThrottled(const base::TimeDelta& throttle_duration) OVERRIDE;
  virtual void OnTypesThrottled(
      ModelTypeSet types,
      const base::TimeDelta& throttle_duration) OVERRIDE;
  virtual bool IsCurrentlyThrottled() OVERRIDE;
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE;
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE;
  virtual void OnReceivedCustomNudgeDelays(
      const std::map<ModelType, base::TimeDelta>& nudge_delays) OVERRIDE;
  virtual void OnReceivedClientInvalidationHintBufferSize(int size) OVERRIDE;
  virtual void OnSyncProtocolError(
      const SyncProtocolError& sync_protocol_error) OVERRIDE;
  virtual void OnReceivedGuRetryDelay(const base::TimeDelta& delay) OVERRIDE;
  virtual void OnReceivedMigrationRequest(syncer::ModelTypeSet types) OVERRIDE;

  // Returns true if the client is currently in exponential backoff.
  bool IsBackingOff() const;

 private:
  enum JobPriority {
    // Non-canary jobs respect exponential backoff.
    NORMAL_PRIORITY,
    // Canary jobs bypass exponential backoff, so use with extreme caution.
    CANARY_PRIORITY
  };

  enum PollAdjustType {
    // Restart the poll interval.
    FORCE_RESET,
    // Restart the poll interval only if its length has changed.
    UPDATE_INTERVAL,
  };

  friend class SyncSchedulerTest;
  friend class SyncSchedulerWhiteboxTest;
  friend class SyncerTest;

  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest, TransientPollFailure);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest,
                           ServerConnectionChangeDuringBackoff);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest,
                           ConnectionChangeCanaryPreemptedByNudge);
  FRIEND_TEST_ALL_PREFIXES(BackoffTriggersSyncSchedulerTest,
                           FailGetEncryptionKey);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest, SuccessfulRetry);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest, FailedRetry);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest, ReceiveNewRetryDelay);

  struct SYNC_EXPORT_PRIVATE WaitInterval {
    enum Mode {
      // Uninitialized state, should not be set in practice.
      UNKNOWN = -1,
      // We enter a series of increasingly longer WaitIntervals if we experience
      // repeated transient failures.  We retry at the end of each interval.
      EXPONENTIAL_BACKOFF,
      // A server-initiated throttled interval.  We do not allow any syncing
      // during such an interval.
      THROTTLED,
    };
    WaitInterval();
    ~WaitInterval();
    WaitInterval(Mode mode, base::TimeDelta length);

    static const char* GetModeString(Mode mode);

    Mode mode;
    base::TimeDelta length;
  };

  static const char* GetModeString(Mode mode);

  void SetDefaultNudgeDelay(base::TimeDelta delay_ms);

  // Invoke the syncer to perform a nudge job.
  void DoNudgeSyncSessionJob(JobPriority priority);

  // Invoke the syncer to perform a configuration job.
  void DoConfigurationSyncSessionJob(JobPriority priority);

  // Helper function for Do{Nudge,Configuration}SyncSessionJob.
  void HandleFailure(
      const sessions::ModelNeutralState& model_neutral_state);

  // Invoke the Syncer to perform a poll job.
  void DoPollSyncSessionJob();

  // Helper function to calculate poll interval.
  base::TimeDelta GetPollInterval();

  // Adjusts the poll timer to account for new poll interval, and possibly
  // resets the poll interval, depedning on the flag's value.
  void AdjustPolling(PollAdjustType type);

  // Helper to restart waiting with |wait_interval_|'s timer.
  void RestartWaiting();

  // Determines if we're allowed to contact the server right now.
  bool CanRunJobNow(JobPriority priority);

  // Determines if we're allowed to contact the server right now.
  bool CanRunNudgeJobNow(JobPriority priority);

  // If the scheduler's current state supports it, this will create a job based
  // on the passed in parameters and coalesce it with any other pending jobs,
  // then post a delayed task to run it.  It may also choose to drop the job or
  // save it for later, depending on the scheduler's current state.
  void ScheduleNudgeImpl(
      const base::TimeDelta& delay,
      const tracked_objects::Location& nudge_location);

  // Helper to signal listeners about changed retry time.
  void NotifyRetryTime(base::Time retry_time);

  // Helper to signal listeners about changed throttled types.
  void NotifyThrottledTypesChanged(ModelTypeSet types);

  // Looks for pending work and, if it finds any, run this work at "canary"
  // priority.
  void TryCanaryJob();

  // At the moment TrySyncSessionJob just posts call to TrySyncSessionJobImpl on
  // current thread. In the future it will request access token here.
  void TrySyncSessionJob();
  void TrySyncSessionJobImpl();

  // Transitions out of the THROTTLED WaitInterval then calls TryCanaryJob().
  void Unthrottle();

  // Called when a per-type throttling interval expires.
  void TypeUnthrottle(base::TimeTicks unthrottle_time);

  // Runs a normal nudge job when the scheduled timer expires.
  void PerformDelayedNudge();

  // Attempts to exit EXPONENTIAL_BACKOFF by calling TryCanaryJob().
  void ExponentialBackoffRetry();

  // Called when the root cause of the current connection error is fixed.
  void OnServerConnectionErrorFixed();

  // Creates a session for a poll and performs the sync.
  void PollTimerCallback();

  // Creates a session for a retry and performs the sync.
  void RetryTimerCallback();

  // Returns the set of types that are enabled and not currently throttled.
  ModelTypeSet GetEnabledAndUnthrottledTypes();

  // Called as we are started to broadcast an initial session snapshot
  // containing data like initial_sync_ended.  Important when the client starts
  // up and does not need to perform an initial sync.
  void SendInitialSnapshot();

  // This is used for histogramming and analysis of ScheduleNudge* APIs.
  // SyncScheduler is the ultimate choke-point for all such invocations (with
  // and without InvalidationState variants, all NudgeSources, etc) and as such
  // is the most flexible place to do this bookkeeping.
  void UpdateNudgeTimeRecords(ModelTypeSet types);

  // For certain methods that need to worry about X-thread posting.
  WeakHandle<SyncSchedulerImpl> weak_handle_this_;

  // Used for logging.
  const std::string name_;

  // Set in Start(), unset in Stop().
  bool started_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  base::TimeDelta syncer_short_poll_interval_seconds_;
  base::TimeDelta syncer_long_poll_interval_seconds_;

  // Periodic timer for polling.  See AdjustPolling.
  base::RepeatingTimer<SyncSchedulerImpl> poll_timer_;

  // The mode of operation.
  Mode mode_;

  // Current wait state.  Null if we're not in backoff and not throttled.
  scoped_ptr<WaitInterval> wait_interval_;

  scoped_ptr<BackoffDelayProvider> delay_provider_;

  // The event that will wake us up.
  base::OneShotTimer<SyncSchedulerImpl> pending_wakeup_timer_;

  // An event that fires when data type throttling expires.
  base::OneShotTimer<SyncSchedulerImpl> type_unthrottle_timer_;

  // Storage for variables related to an in-progress configure request.  Note
  // that (mode_ != CONFIGURATION_MODE) \implies !pending_configure_params_.
  scoped_ptr<ConfigurationParams> pending_configure_params_;

  // If we have a nudge pending to run soon, it will be listed here.
  base::TimeTicks scheduled_nudge_time_;

  // Keeps track of work that the syncer needs to handle.
  sessions::NudgeTracker nudge_tracker_;

  // Invoked to run through the sync cycle.
  scoped_ptr<Syncer> syncer_;

  sessions::SyncSessionContext* session_context_;

  // A map tracking LOCAL NudgeSource invocations of ScheduleNudge* APIs,
  // organized by datatype. Each datatype that was part of the types requested
  // in the call will have its TimeTicks value updated.
  typedef std::map<ModelType, base::TimeTicks> ModelTypeTimeMap;
  ModelTypeTimeMap last_local_nudges_by_model_type_;

  // Used as an "anti-reentrancy defensive assertion".
  // While true, it is illegal for any new scheduling activity to take place.
  // Ensures that higher layers don't break this law in response to events that
  // take place during a sync cycle. We call this out because such violations
  // could result in tight sync loops hitting sync servers.
  bool no_scheduling_allowed_;

  // crbug/251307. This is a workaround for M29. crbug/259913 tracks proper fix
  // for M30.
  // The issue is that poll job runs after few hours of inactivity and therefore
  // will always fail with auth error because of expired access token. Once
  // fresh access token is requested poll job is not retried.
  // The change is to remember that poll timer just fired and retry poll job
  // after credentials are updated.
  bool do_poll_after_credentials_updated_;

  // TryJob might get called for multiple reasons. It should only call
  // DoPollSyncSessionJob after some time since the last attempt.
  // last_poll_reset_ keeps track of when was last attempt.
  base::TimeTicks last_poll_reset_;

  // next_sync_session_job_priority_ defines which priority will be used next
  // time TrySyncSessionJobImpl is called. CANARY_PRIORITY allows syncer to run
  // even if scheduler is in exponential backoff. This is needed for events that
  // have chance of resolving previous error (e.g. network connection change
  // after NETWORK_UNAVAILABLE error).
  // It is reset back to NORMAL_PRIORITY on every call to TrySyncSessionJobImpl.
  JobPriority next_sync_session_job_priority_;

  // One-shot timer for scheduling GU retry according to delay set by server.
  base::OneShotTimer<SyncSchedulerImpl> retry_timer_;

  base::WeakPtrFactory<SyncSchedulerImpl> weak_ptr_factory_;

  // A second factory specially for weak_handle_this_, to allow the handle
  // to be const and alleviate threading concerns.
  base::WeakPtrFactory<SyncSchedulerImpl> weak_ptr_factory_for_weak_handle_;

  DISALLOW_COPY_AND_ASSIGN(SyncSchedulerImpl);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_
