// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/nudge_source.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/sessions/sync_session.h"
#include "sync/sessions/sync_session_context.h"

namespace syncer {

class BackoffDelayProvider;

class SyncSchedulerImpl : public SyncScheduler {
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
      const base::TimeDelta& delay,
      NudgeSource source,
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void ScheduleNudgeWithStatesAsync(
      const base::TimeDelta& delay, NudgeSource source,
      const ModelTypeStateMap& type_state_map,
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

 private:
  enum JobProcessDecision {
    // Indicates we should continue with the current job.
    CONTINUE,
    // Indicates that we should save it to be processed later.
    SAVE,
    // Indicates we should drop this job.
    DROP,
  };

  struct SyncSessionJob {
    // An enum used to describe jobs for scheduling purposes.
    enum SyncSessionJobPurpose {
      // Uninitialized state, should never be hit in practice.
      UNKNOWN = -1,
      // Our poll timer schedules POLL jobs periodically based on a server
      // assigned poll interval.
      POLL,
      // A nudge task can come from a variety of components needing to force
      // a sync.  The source is inferable from |session.source()|.
      NUDGE,
      // Typically used for fetching updates for a subset of the enabled types
      // during initial sync or reconfiguration.
      CONFIGURATION,
    };
    SyncSessionJob();
    SyncSessionJob(SyncSessionJobPurpose purpose, base::TimeTicks start,
        linked_ptr<sessions::SyncSession> session, bool is_canary_job,
        const ConfigurationParams& config_params,
        const tracked_objects::Location& nudge_location);
    ~SyncSessionJob();
    static const char* GetPurposeString(SyncSessionJobPurpose purpose);

    SyncSessionJobPurpose purpose;
    base::TimeTicks scheduled_start;
    linked_ptr<sessions::SyncSession> session;
    bool is_canary_job;
    ConfigurationParams config_params;

    // This is the location the job came from.  Used for debugging.
    // In case of multiple nudges getting coalesced this stores the
    // first location that came in.
    tracked_objects::Location from_here;
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

  struct WaitInterval {
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
    // expose the pointer here.
    scoped_ptr<SyncSessionJob> pending_configure_job;
  };

  static const char* GetModeString(Mode mode);

  static const char* GetDecisionString(JobProcessDecision decision);

  // Assign |start| and |end| to appropriate SyncerStep values for the
  // specified |purpose|.
  static void SetSyncerStepsForPurpose(
      SyncSessionJob::SyncSessionJobPurpose purpose,
      SyncerStep* start, SyncerStep* end);

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
  void ScheduleSyncSessionJob(const SyncSessionJob& job);

  // Invoke the Syncer to perform a sync.
  void DoSyncSessionJob(const SyncSessionJob& job);

  // Called after the Syncer has performed the sync represented by |job|, to
  // reset our state.
  void FinishSyncSessionJob(const SyncSessionJob& job);

  // Helper to FinishSyncSessionJob to schedule the next sync operation.
  void ScheduleNextSync(const SyncSessionJob& old_job);

  // Helper to configure polling intervals. Used by Start and ScheduleNextSync.
  void AdjustPolling(const SyncSessionJob* old_job);

  // Helper to restart waiting with |wait_interval_|'s timer.
  void RestartWaiting();

  // Helper to ScheduleNextSync in case of consecutive sync errors.
  void HandleContinuationError(const SyncSessionJob& old_job);

  // Determines if it is legal to run |job| by checking current
  // operational mode, backoff or throttling, freshness
  // (so we don't make redundant syncs), and connection.
  bool ShouldRunJob(const SyncSessionJob& job);

  // Decide whether we should CONTINUE, SAVE or DROP the job.
  JobProcessDecision DecideOnJob(const SyncSessionJob& job);

  // Decide on whether to CONTINUE, SAVE or DROP the job when we are in
  // backoff mode.
  JobProcessDecision DecideWhileInWaitInterval(const SyncSessionJob& job);

  // Saves the job for future execution. Note: It drops all the poll jobs.
  void SaveJob(const SyncSessionJob& job);

  // Coalesces the current job with the pending nudge.
  void InitOrCoalescePendingJob(const SyncSessionJob& job);

  // 'Impl' here refers to real implementation of public functions, running on
  // |thread_|.
  void StopImpl(const base::Closure& callback);
  void ScheduleNudgeImpl(
      const base::TimeDelta& delay,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
      const ModelTypeStateMap& type_state_map,
      bool is_canary_job, const tracked_objects::Location& nudge_location);

  // Returns true if the client is currently in exponential backoff.
  bool IsBackingOff() const;

  // Helper to signal all listeners registered with |session_context_|.
  void Notify(SyncEngineEvent::EventCause cause);

  // Callback to change backoff state.
  void DoCanaryJob();
  void Unthrottle();

  // Executes the pending job. Called whenever an event occurs that may
  // change conditions permitting a job to run. Like when network connection is
  // re-established, mode changes etc.
  void DoPendingJobIfPossible(bool is_canary_job);

  // Called when the root cause of the current connection error is fixed.
  void OnServerConnectionErrorFixed();

  // The pointer is owned by the caller.
  sessions::SyncSession* CreateSyncSession(
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

  // Tracks in-flight nudges so we can coalesce.
  scoped_ptr<SyncSessionJob> pending_nudge_;

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
