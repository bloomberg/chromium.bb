// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_
#define SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "sync/base/sync_export.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/nudge_source.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/sync_session_job.h"
#include "sync/engine/syncer.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/sessions/sync_session.h"
#include "sync/sessions/sync_session_context.h"

namespace syncer {

class BackoffDelayProvider;

class SYNC_EXPORT_PRIVATE SyncSchedulerImpl :
    public SyncScheduler,
    public SyncSessionJob::DestructionObserver {
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
  virtual bool ScheduleConfiguration(
      const ConfigurationParams& params) OVERRIDE;
  virtual void RequestStop(const base::Closure& callback) OVERRIDE;
  virtual void ScheduleNudgeAsync(
      const base::TimeDelta& desired_delay,
      NudgeSource source,
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void ScheduleNudgeWithStatesAsync(
      const base::TimeDelta& desired_delay, NudgeSource source,
      const ModelTypeInvalidationMap& invalidation_map,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void SetNotificationsEnabled(bool notifications_enabled) OVERRIDE;

  virtual base::TimeDelta GetSessionsCommitDelay() const OVERRIDE;

  virtual void OnCredentialsUpdated() OVERRIDE;
  virtual void OnConnectionStatusChange() OVERRIDE;

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(
      const base::TimeTicks& silenced_until) OVERRIDE;
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE;
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE;
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE;
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE;
  virtual void OnShouldStopSyncingPermanently() OVERRIDE;
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& snapshot) OVERRIDE;

  // SyncSessionJob::DestructionObserver implementation.
  virtual void OnJobDestroyed(SyncSessionJob* job) OVERRIDE;

 private:
  enum JobProcessDecision {
    // Indicates we should continue with the current job.
    CONTINUE,
    // Indicates that we should save it to be processed later.
    SAVE,
    // Indicates we should drop this job.
    DROP,
  };

  friend class SyncSchedulerTest;
  friend class SyncSchedulerWhiteboxTest;
  friend class SyncerTest;

  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
      DropNudgeWhileExponentialBackOff);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, SaveNudge);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           SaveNudgeWhileTypeThrottled);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, ContinueNudge);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, DropPoll);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, ContinuePoll);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, ContinueConfiguration);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           SaveConfigurationWhileThrottled);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           SaveNudgeWhileThrottled);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           ContinueCanaryJobConfig);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
      ContinueNudgeWhileExponentialBackOff);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest, TransientPollFailure);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest, GetInitialBackoffDelay);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest,
                           ServerConnectionChangeDuringBackoff);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerTest,
                           ConnectionChangeCanaryPreemptedByNudge);

  struct SYNC_EXPORT_PRIVATE WaitInterval {
    enum Mode {
      // Uninitialized state, should not be set in practice.
      UNKNOWN = -1,
      // A wait interval whose duration has been affected by exponential
      // backoff.
      // EXPONENTIAL_BACKOFF intervals are nudge-rate limited to 1 per interval.
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

    // This bool is set to true if we have observed a nudge during this
    // interval and mode == EXPONENTIAL_BACKOFF.
    bool had_nudge;
    base::TimeDelta length;
    base::OneShotTimer<SyncSchedulerImpl> timer;

    // Configure jobs are saved only when backing off or throttling. So we
    // expose the pointer here (does not own, similar to pending_nudge).
    SyncSessionJob* pending_configure_job;
  };

  static const char* GetModeString(Mode mode);

  static const char* GetDecisionString(JobProcessDecision decision);

  // Helpers that log before posting to |sync_loop_|.  These will only post
  // the task in between calls to Start/Stop.
  void PostTask(const tracked_objects::Location& from_here,
                const char* name,
                const base::Closure& task);
  void PostDelayedTask(const tracked_objects::Location& from_here,
                       const char* name,
                       const base::Closure& task,
                       base::TimeDelta delay);

  // Helper to assemble a job and post a delayed task to sync.
  void ScheduleSyncSessionJob(const tracked_objects::Location& loc,
                              scoped_ptr<SyncSessionJob> job);

  // Invoke the Syncer to perform a sync.
  bool DoSyncSessionJob(scoped_ptr<SyncSessionJob> job);

  // Called after the Syncer has performed the sync represented by |job|, to
  // reset our state.  |exited_prematurely| is true if the Syncer did not
  // cycle from job.start_step() to job.end_step(), likely because the
  // scheduler was forced to quit the job mid-way through.
  bool FinishSyncSessionJob(scoped_ptr<SyncSessionJob> job,
                            bool exited_prematurely);

  // Helper to FinishSyncSessionJob to schedule the next sync operation.
  // |succeeded| carries the return value of |old_job|->Finish.
  void ScheduleNextSync(scoped_ptr<SyncSessionJob> finished_job,
                        bool succeeded);

  // Helper to configure polling intervals. Used by Start and ScheduleNextSync.
  void AdjustPolling(const SyncSessionJob* old_job);

  // Helper to restart waiting with |wait_interval_|'s timer.
  void RestartWaiting(scoped_ptr<SyncSessionJob> job);

  // Helper to ScheduleNextSync in case of consecutive sync errors.
  void HandleContinuationError(scoped_ptr<SyncSessionJob> old_job);

  // Decide whether we should CONTINUE, SAVE or DROP the job.
  JobProcessDecision DecideOnJob(const SyncSessionJob& job);

  // If DecideOnJob decides that |job| should be SAVEd, this function will
  // carry out the task of actually "saving" (or coalescing) the job.
  void HandleSaveJobDecision(scoped_ptr<SyncSessionJob> job);

  // Decide on whether to CONTINUE, SAVE or DROP the job when we are in
  // backoff mode.
  JobProcessDecision DecideWhileInWaitInterval(const SyncSessionJob& job);

  // 'Impl' here refers to real implementation of public functions, running on
  // |thread_|.
  void StopImpl(const base::Closure& callback);
  void ScheduleNudgeImpl(
      const base::TimeDelta& delay,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
      const ModelTypeInvalidationMap& invalidation_map,
      const tracked_objects::Location& nudge_location);

  // Returns true if the client is currently in exponential backoff.
  bool IsBackingOff() const;

  // Helper to signal all listeners registered with |session_context_|.
  void Notify(SyncEngineEvent::EventCause cause);

  // Helper to signal listeners about changed retry time
  void NotifyRetryTime(base::Time retry_time);

  // Callback to change backoff state. |to_be_canary| in both cases is the job
  // that should be granted canary privileges. Note: it is possible that the
  // job that gets scheduled when this callback is scheduled is different from
  // the job that will actually get executed, because other jobs may have been
  // scheduled while we were waiting for the callback.
  void DoCanaryJob(scoped_ptr<SyncSessionJob> to_be_canary);
  void Unthrottle(scoped_ptr<SyncSessionJob> to_be_canary);

  // Returns a pending job that has potential to run given the state of the
  // scheduler, if it exists. Useful whenever an event occurs that may
  // change conditions that permit a job to run, such as re-establishing
  // network connection, auth refresh, mode changes etc. Note that the returned
  // job may have been scheduled to run at a later time, or may have been
  // unscheduled.  In the former case, this will result in abandoning the old
  // job and effectively cancelling it.
  scoped_ptr<SyncSessionJob> TakePendingJobForCurrentMode();

  // Called when the root cause of the current connection error is fixed.
  void OnServerConnectionErrorFixed();

  scoped_ptr<sessions::SyncSession> CreateSyncSession(
      const sessions::SyncSourceInfo& info);

  // Creates a session for a poll and performs the sync.
  void PollTimerCallback();

  // Used to update |connection_code_|, see below.
  void UpdateServerConnectionManagerStatus(
      HttpResponse::ServerConnectionCode code);

  // Called once the first time thread_ is started to broadcast an initial
  // session snapshot containing data like initial_sync_ended.  Important when
  // the client starts up and does not need to perform an initial sync.
  void SendInitialSnapshot();

  // This is used for histogramming and analysis of ScheduleNudge* APIs.
  // SyncScheduler is the ultimate choke-point for all such invocations (with
  // and without InvalidationState variants, all NudgeSources, etc) and as such
  // is the most flexible place to do this bookkeeping.
  void UpdateNudgeTimeRecords(const sessions::SyncSourceInfo& info);

  virtual void OnActionableError(const sessions::SyncSessionSnapshot& snapshot);

  void set_pending_nudge(SyncSessionJob* job);

  base::WeakPtrFactory<SyncSchedulerImpl> weak_ptr_factory_;

  // A second factory specially for weak_handle_this_, to allow the handle
  // to be const and alleviate threading concerns.
  base::WeakPtrFactory<SyncSchedulerImpl> weak_ptr_factory_for_weak_handle_;

  // For certain methods that need to worry about X-thread posting.
  const WeakHandle<SyncSchedulerImpl> weak_handle_this_;

  // Used for logging.
  const std::string name_;

  // The message loop this object is on.  Almost all methods have to
  // be called on this thread.
  MessageLoop* const sync_loop_;

  // Set in Start(), unset in Stop().
  bool started_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  base::TimeDelta syncer_short_poll_interval_seconds_;
  base::TimeDelta syncer_long_poll_interval_seconds_;

  // Server-tweakable sessions commit delay.
  base::TimeDelta sessions_commit_delay_;

  // Periodic timer for polling.  See AdjustPolling.
  base::RepeatingTimer<SyncSchedulerImpl> poll_timer_;

  // The mode of operation.
  Mode mode_;

  // The latest connection code we got while trying to connect.
  HttpResponse::ServerConnectionCode connection_code_;

  // Tracks (does not own) in-flight nudges (scheduled or unscheduled),
  // so we can coalesce. NULL if there is no pending nudge.
  SyncSessionJob* pending_nudge_;

  // There are certain situations where we want to remember a nudge, but
  // there is no well defined moment in time in the future when that nudge
  // should run, e.g. if it requires a mode switch or updated auth credentials.
  // This member will own NUDGE jobs in those cases, until an external event
  // (mode switch or fixed auth) occurs to trigger a retry.  Should be treated
  // as opaque / not interacted with (i.e. we could build a wrapper to
  // hide the type, but that's probably overkill).
  scoped_ptr<SyncSessionJob> unscheduled_nudge_storage_;

  // Current wait state.  Null if we're not in backoff and not throttled.
  scoped_ptr<WaitInterval> wait_interval_;

  scoped_ptr<BackoffDelayProvider> delay_provider_;

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

  DISALLOW_COPY_AND_ASSIGN(SyncSchedulerImpl);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNC_SCHEDULER_IMPL_H_
