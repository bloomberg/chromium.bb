// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_scheduler_impl.h"

#include <algorithm>
#include <cstring>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "sync/engine/backoff_delay_provider.h"
#include "sync/engine/syncer.h"
#include "sync/protocol/proto_enum_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/data_type_histogram.h"
#include "sync/util/logging.h"

using base::TimeDelta;
using base::TimeTicks;

namespace syncer {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sync_pb::GetUpdatesCallerInfo;

namespace {

bool ShouldRequestEarlyExit(const SyncProtocolError& error) {
  switch (error.error_type) {
    case SYNC_SUCCESS:
    case MIGRATION_DONE:
    case THROTTLED:
    case TRANSIENT_ERROR:
      return false;
    case NOT_MY_BIRTHDAY:
    case CLEAR_PENDING:
    case DISABLED_BY_ADMIN:
    case USER_ROLLBACK:
      // If we send terminate sync early then |sync_cycle_ended| notification
      // would not be sent. If there were no actions then |ACTIONABLE_ERROR|
      // notification wouldnt be sent either. Then the UI layer would be left
      // waiting forever. So assert we would send something.
      DCHECK_NE(error.action, UNKNOWN_ACTION);
      return true;
    case INVALID_CREDENTIAL:
      // The notification for this is handled by PostAndProcessHeaders|.
      // Server does no have to send any action for this.
      return true;
    // Make the default a NOTREACHED. So if a new error is introduced we
    // think about its expected functionality.
    default:
      NOTREACHED();
      return false;
  }
}

bool IsActionableError(
    const SyncProtocolError& error) {
  return (error.action != UNKNOWN_ACTION);
}
}  // namespace

ConfigurationParams::ConfigurationParams()
    : source(GetUpdatesCallerInfo::UNKNOWN) {}
ConfigurationParams::ConfigurationParams(
    const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& source,
    ModelTypeSet types_to_download,
    const ModelSafeRoutingInfo& routing_info,
    const base::Closure& ready_task,
    const base::Closure& retry_task)
    : source(source),
      types_to_download(types_to_download),
      routing_info(routing_info),
      ready_task(ready_task),
      retry_task(retry_task) {
  DCHECK(!ready_task.is_null());
  DCHECK(!retry_task.is_null());
}
ConfigurationParams::~ConfigurationParams() {}

SyncSchedulerImpl::WaitInterval::WaitInterval()
    : mode(UNKNOWN) {}

SyncSchedulerImpl::WaitInterval::WaitInterval(Mode mode, TimeDelta length)
    : mode(mode), length(length) {}

SyncSchedulerImpl::WaitInterval::~WaitInterval() {}

#define ENUM_CASE(x) case x: return #x; break;

const char* SyncSchedulerImpl::WaitInterval::GetModeString(Mode mode) {
  switch (mode) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(EXPONENTIAL_BACKOFF);
    ENUM_CASE(THROTTLED);
  }
  NOTREACHED();
  return "";
}

GetUpdatesCallerInfo::GetUpdatesSource GetUpdatesFromNudgeSource(
    NudgeSource source) {
  switch (source) {
    case NUDGE_SOURCE_NOTIFICATION:
      return GetUpdatesCallerInfo::NOTIFICATION;
    case NUDGE_SOURCE_LOCAL:
      return GetUpdatesCallerInfo::LOCAL;
    case NUDGE_SOURCE_LOCAL_REFRESH:
      return GetUpdatesCallerInfo::DATATYPE_REFRESH;
    case NUDGE_SOURCE_UNKNOWN:
      return GetUpdatesCallerInfo::UNKNOWN;
    default:
      NOTREACHED();
      return GetUpdatesCallerInfo::UNKNOWN;
  }
}

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

#define SDVLOG_LOC(from_here, verbose_level)             \
  DVLOG_LOC(from_here, verbose_level) << name_ << ": "

namespace {

const int kDefaultSessionsCommitDelaySeconds = 10;

bool IsConfigRelatedUpdateSourceValue(
    GetUpdatesCallerInfo::GetUpdatesSource source) {
  switch (source) {
    case GetUpdatesCallerInfo::RECONFIGURATION:
    case GetUpdatesCallerInfo::MIGRATION:
    case GetUpdatesCallerInfo::NEW_CLIENT:
    case GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE:
      return true;
    default:
      return false;
  }
}

}  // namespace

SyncSchedulerImpl::SyncSchedulerImpl(const std::string& name,
                                     BackoffDelayProvider* delay_provider,
                                     sessions::SyncSessionContext* context,
                                     Syncer* syncer)
    : name_(name),
      started_(false),
      syncer_short_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds)),
      syncer_long_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultLongPollIntervalSeconds)),
      sessions_commit_delay_(
          TimeDelta::FromSeconds(kDefaultSessionsCommitDelaySeconds)),
      mode_(NORMAL_MODE),
      delay_provider_(delay_provider),
      syncer_(syncer),
      session_context_(context),
      no_scheduling_allowed_(false),
      do_poll_after_credentials_updated_(false),
      next_sync_session_job_priority_(NORMAL_PRIORITY),
      weak_ptr_factory_(this),
      weak_ptr_factory_for_weak_handle_(this) {
  weak_handle_this_ = MakeWeakHandle(
      weak_ptr_factory_for_weak_handle_.GetWeakPtr());
}

SyncSchedulerImpl::~SyncSchedulerImpl() {
  DCHECK(CalledOnValidThread());
  Stop();
}

void SyncSchedulerImpl::OnCredentialsUpdated() {
  DCHECK(CalledOnValidThread());

  if (HttpResponse::SYNC_AUTH_ERROR ==
      session_context_->connection_manager()->server_status()) {
    OnServerConnectionErrorFixed();
  }
}

void SyncSchedulerImpl::OnConnectionStatusChange() {
  if (HttpResponse::CONNECTION_UNAVAILABLE  ==
      session_context_->connection_manager()->server_status()) {
    // Optimistically assume that the connection is fixed and try
    // connecting.
    OnServerConnectionErrorFixed();
  }
}

void SyncSchedulerImpl::OnServerConnectionErrorFixed() {
  // There could be a pending nudge or configuration job in several cases:
  //
  // 1. We're in exponential backoff.
  // 2. We're silenced / throttled.
  // 3. A nudge was saved previously due to not having a valid auth token.
  // 4. A nudge was scheduled + saved while in configuration mode.
  //
  // In all cases except (2), we want to retry contacting the server. We
  // call TryCanaryJob to achieve this, and note that nothing -- not even a
  // canary job -- can bypass a THROTTLED WaitInterval. The only thing that
  // has the authority to do that is the Unthrottle timer.
  TryCanaryJob();
}

void SyncSchedulerImpl::Start(Mode mode) {
  DCHECK(CalledOnValidThread());
  std::string thread_name = base::MessageLoop::current()->thread_name();
  if (thread_name.empty())
    thread_name = "<Main thread>";
  SDVLOG(2) << "Start called from thread "
            << thread_name << " with mode " << GetModeString(mode);
  if (!started_) {
    started_ = true;
    SendInitialSnapshot();
  }

  DCHECK(!session_context_->account_name().empty());
  DCHECK(syncer_.get());
  Mode old_mode = mode_;
  mode_ = mode;
  AdjustPolling(UPDATE_INTERVAL);  // Will kick start poll timer if needed.

  if (old_mode != mode_ && mode_ == NORMAL_MODE) {
    // We just got back to normal mode.  Let's try to run the work that was
    // queued up while we were configuring.

    // Update our current time before checking IsRetryRequired().
    nudge_tracker_.SetSyncCycleStartTime(base::TimeTicks::Now());
    if (nudge_tracker_.IsSyncRequired() && CanRunNudgeJobNow(NORMAL_PRIORITY)) {
      TrySyncSessionJob();
    }
  }
}

ModelTypeSet SyncSchedulerImpl::GetEnabledAndUnthrottledTypes() {
  ModelTypeSet enabled_types = session_context_->GetEnabledTypes();
  ModelTypeSet enabled_protocol_types =
      Intersection(ProtocolTypes(), enabled_types);
  ModelTypeSet throttled_types = nudge_tracker_.GetThrottledTypes();
  return Difference(enabled_protocol_types, throttled_types);
}

void SyncSchedulerImpl::SendInitialSnapshot() {
  DCHECK(CalledOnValidThread());
  scoped_ptr<SyncSession> dummy(SyncSession::Build(session_context_, this));
  SyncCycleEvent event(SyncCycleEvent::STATUS_CHANGED);
  event.snapshot = dummy->TakeSnapshot();
  FOR_EACH_OBSERVER(SyncEngineEventListener,
                    *session_context_->listeners(),
                    OnSyncCycleEvent(event));
}

namespace {

// Helper to extract the routing info corresponding to types in
// |types_to_download| from |current_routes|.
void BuildModelSafeParams(
    ModelTypeSet types_to_download,
    const ModelSafeRoutingInfo& current_routes,
    ModelSafeRoutingInfo* result_routes) {
  for (ModelTypeSet::Iterator iter = types_to_download.First(); iter.Good();
       iter.Inc()) {
    ModelType type = iter.Get();
    ModelSafeRoutingInfo::const_iterator route = current_routes.find(type);
    DCHECK(route != current_routes.end());
    ModelSafeGroup group = route->second;
    (*result_routes)[type] = group;
  }
}

}  // namespace.

void SyncSchedulerImpl::ScheduleConfiguration(
    const ConfigurationParams& params) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsConfigRelatedUpdateSourceValue(params.source));
  DCHECK_EQ(CONFIGURATION_MODE, mode_);
  DCHECK(!params.ready_task.is_null());
  CHECK(started_) << "Scheduler must be running to configure.";
  SDVLOG(2) << "Reconfiguring syncer.";

  // Only one configuration is allowed at a time. Verify we're not waiting
  // for a pending configure job.
  DCHECK(!pending_configure_params_);

  ModelSafeRoutingInfo restricted_routes;
  BuildModelSafeParams(params.types_to_download,
                       params.routing_info,
                       &restricted_routes);
  session_context_->SetRoutingInfo(restricted_routes);

  // Only reconfigure if we have types to download.
  if (!params.types_to_download.Empty()) {
    pending_configure_params_.reset(new ConfigurationParams(params));
    TrySyncSessionJob();
  } else {
    SDVLOG(2) << "No change in routing info, calling ready task directly.";
    params.ready_task.Run();
  }
}

bool SyncSchedulerImpl::CanRunJobNow(JobPriority priority) {
  DCHECK(CalledOnValidThread());
  if (wait_interval_ && wait_interval_->mode == WaitInterval::THROTTLED) {
    SDVLOG(1) << "Unable to run a job because we're throttled.";
    return false;
  }

  if (wait_interval_
      && wait_interval_->mode == WaitInterval::EXPONENTIAL_BACKOFF
      && priority != CANARY_PRIORITY) {
    SDVLOG(1) << "Unable to run a job because we're backing off.";
    return false;
  }

  if (session_context_->connection_manager()->HasInvalidAuthToken()) {
    SDVLOG(1) << "Unable to run a job because we have no valid auth token.";
    return false;
  }

  return true;
}

bool SyncSchedulerImpl::CanRunNudgeJobNow(JobPriority priority) {
  DCHECK(CalledOnValidThread());

  if (!CanRunJobNow(priority)) {
    SDVLOG(1) << "Unable to run a nudge job right now";
    return false;
  }

  const ModelTypeSet enabled_types = session_context_->GetEnabledTypes();
  if (nudge_tracker_.GetThrottledTypes().HasAll(enabled_types)) {
    SDVLOG(1) << "Not running a nudge because we're fully type throttled.";
    return false;
  }

  if (mode_ == CONFIGURATION_MODE) {
    SDVLOG(1) << "Not running nudge because we're in configuration mode.";
    return false;
  }

  return true;
}

void SyncSchedulerImpl::ScheduleLocalNudge(
    const TimeDelta& desired_delay,
    ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(!types.Empty());

  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling sync because of local change to "
      << ModelTypeSetToString(types);
  UpdateNudgeTimeRecords(types);
  nudge_tracker_.RecordLocalChange(types);
  ScheduleNudgeImpl(desired_delay, nudge_location);
}

void SyncSchedulerImpl::ScheduleLocalRefreshRequest(
    const TimeDelta& desired_delay,
    ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(!types.Empty());

  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling sync because of local refresh request for "
      << ModelTypeSetToString(types);
  nudge_tracker_.RecordLocalRefreshRequest(types);
  ScheduleNudgeImpl(desired_delay, nudge_location);
}

void SyncSchedulerImpl::ScheduleInvalidationNudge(
    const TimeDelta& desired_delay,
    syncer::ModelType model_type,
    scoped_ptr<InvalidationInterface> invalidation,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());

  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling sync because we received invalidation for "
      << ModelTypeToString(model_type);
  nudge_tracker_.RecordRemoteInvalidation(model_type, invalidation.Pass());
  ScheduleNudgeImpl(desired_delay, nudge_location);
}

void SyncSchedulerImpl::ScheduleInitialSyncNudge(syncer::ModelType model_type) {
  DCHECK(CalledOnValidThread());

  SDVLOG(2) << "Scheduling non-blocking initial sync for "
            << ModelTypeToString(model_type);
  nudge_tracker_.RecordInitialSyncRequired(model_type);
  ScheduleNudgeImpl(TimeDelta::FromSeconds(0), FROM_HERE);
}

// TODO(zea): Consider adding separate throttling/backoff for datatype
// refresh requests.
void SyncSchedulerImpl::ScheduleNudgeImpl(
    const TimeDelta& delay,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());

  if (no_scheduling_allowed_) {
    NOTREACHED() << "Illegal to schedule job while session in progress.";
    return;
  }

  if (!started_) {
    SDVLOG_LOC(nudge_location, 2)
        << "Dropping nudge, scheduler is not running.";
    return;
  }

  SDVLOG_LOC(nudge_location, 2)
      << "In ScheduleNudgeImpl with delay "
      << delay.InMilliseconds() << " ms";

  if (!CanRunNudgeJobNow(NORMAL_PRIORITY))
    return;

  TimeTicks incoming_run_time = TimeTicks::Now() + delay;
  if (!scheduled_nudge_time_.is_null() &&
    (scheduled_nudge_time_ < incoming_run_time)) {
    // Old job arrives sooner than this one.  Don't reschedule it.
    return;
  }

  // Either there is no existing nudge in flight or the incoming nudge should be
  // made to arrive first (preempt) the existing nudge.  We reschedule in either
  // case.
  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling a nudge with "
      << delay.InMilliseconds() << " ms delay";
  scheduled_nudge_time_ = incoming_run_time;
  pending_wakeup_timer_.Start(
      nudge_location,
      delay,
      base::Bind(&SyncSchedulerImpl::PerformDelayedNudge,
                 weak_ptr_factory_.GetWeakPtr()));
}

const char* SyncSchedulerImpl::GetModeString(SyncScheduler::Mode mode) {
  switch (mode) {
    ENUM_CASE(CONFIGURATION_MODE);
    ENUM_CASE(NORMAL_MODE);
  }
  return "";
}

void SyncSchedulerImpl::DoNudgeSyncSessionJob(JobPriority priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(CanRunNudgeJobNow(priority));

  DVLOG(2) << "Will run normal mode sync cycle with types "
           << ModelTypeSetToString(session_context_->GetEnabledTypes());
  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  bool premature_exit = !syncer_->NormalSyncShare(
      GetEnabledAndUnthrottledTypes(),
      nudge_tracker_,
      session.get());
  AdjustPolling(FORCE_RESET);
  // Don't run poll job till the next time poll timer fires.
  do_poll_after_credentials_updated_ = false;

  bool success = !premature_exit
      && !sessions::HasSyncerError(
          session->status_controller().model_neutral_state());

  if (success) {
    // That cycle took care of any outstanding work we had.
    SDVLOG(2) << "Nudge succeeded.";
    nudge_tracker_.RecordSuccessfulSyncCycle();
    scheduled_nudge_time_ = base::TimeTicks();

    // If we're here, then we successfully reached the server.  End all backoff.
    wait_interval_.reset();
    NotifyRetryTime(base::Time());
  } else {
    HandleFailure(session->status_controller().model_neutral_state());
  }
}

void SyncSchedulerImpl::DoConfigurationSyncSessionJob(JobPriority priority) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(mode_, CONFIGURATION_MODE);
  DCHECK(pending_configure_params_ != NULL);

  if (!CanRunJobNow(priority)) {
    SDVLOG(2) << "Unable to run configure job right now.";
    if (!pending_configure_params_->retry_task.is_null()) {
      pending_configure_params_->retry_task.Run();
      pending_configure_params_->retry_task.Reset();
    }
    return;
  }

  SDVLOG(2) << "Will run configure SyncShare with types "
            << ModelTypeSetToString(session_context_->GetEnabledTypes());
  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  bool premature_exit = !syncer_->ConfigureSyncShare(
      pending_configure_params_->types_to_download,
      pending_configure_params_->source,
      session.get());
  AdjustPolling(FORCE_RESET);
  // Don't run poll job till the next time poll timer fires.
  do_poll_after_credentials_updated_ = false;

  bool success = !premature_exit
      && !sessions::HasSyncerError(
          session->status_controller().model_neutral_state());

  if (success) {
    SDVLOG(2) << "Configure succeeded.";
    pending_configure_params_->ready_task.Run();
    pending_configure_params_.reset();

    // If we're here, then we successfully reached the server.  End all backoff.
    wait_interval_.reset();
    NotifyRetryTime(base::Time());
  } else {
    HandleFailure(session->status_controller().model_neutral_state());
    // Sync cycle might receive response from server that causes scheduler to
    // stop and draws pending_configure_params_ invalid.
    if (started_ && !pending_configure_params_->retry_task.is_null()) {
      pending_configure_params_->retry_task.Run();
      pending_configure_params_->retry_task.Reset();
    }
  }
}

void SyncSchedulerImpl::HandleFailure(
    const sessions::ModelNeutralState& model_neutral_state) {
  if (IsCurrentlyThrottled()) {
    SDVLOG(2) << "Was throttled during previous sync cycle.";
    RestartWaiting();
  } else if (!IsBackingOff()) {
    // Setup our backoff if this is our first such failure.
    TimeDelta length = delay_provider_->GetDelay(
        delay_provider_->GetInitialDelay(model_neutral_state));
    wait_interval_.reset(
        new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF, length));
    SDVLOG(2) << "Sync cycle failed.  Will back off for "
        << wait_interval_->length.InMilliseconds() << "ms.";
    RestartWaiting();
  }
}

void SyncSchedulerImpl::DoPollSyncSessionJob() {
  base::AutoReset<bool> protector(&no_scheduling_allowed_, true);

  SDVLOG(2) << "Polling with types "
            << ModelTypeSetToString(GetEnabledAndUnthrottledTypes());
  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  syncer_->PollSyncShare(
      GetEnabledAndUnthrottledTypes(),
      session.get());

  AdjustPolling(FORCE_RESET);

  if (IsCurrentlyThrottled()) {
    SDVLOG(2) << "Poll request got us throttled.";
    // The OnSilencedUntil() call set up the WaitInterval for us.  All we need
    // to do is start the timer.
    RestartWaiting();
  }
}

void SyncSchedulerImpl::UpdateNudgeTimeRecords(ModelTypeSet types) {
  DCHECK(CalledOnValidThread());
  base::TimeTicks now = TimeTicks::Now();
  // Update timing information for how often datatypes are triggering nudges.
  for (ModelTypeSet::Iterator iter = types.First(); iter.Good(); iter.Inc()) {
    base::TimeTicks previous = last_local_nudges_by_model_type_[iter.Get()];
    last_local_nudges_by_model_type_[iter.Get()] = now;
    if (previous.is_null())
      continue;

#define PER_DATA_TYPE_MACRO(type_str) \
    SYNC_FREQ_HISTOGRAM("Sync.Freq" type_str, now - previous);
    SYNC_DATA_TYPE_HISTOGRAM(iter.Get());
#undef PER_DATA_TYPE_MACRO
  }
}

TimeDelta SyncSchedulerImpl::GetPollInterval() {
  return (!session_context_->notifications_enabled() ||
          !session_context_->ShouldFetchUpdatesBeforeCommit()) ?
      syncer_short_poll_interval_seconds_ :
      syncer_long_poll_interval_seconds_;
}

void SyncSchedulerImpl::AdjustPolling(PollAdjustType type) {
  DCHECK(CalledOnValidThread());

  TimeDelta poll = GetPollInterval();
  bool rate_changed = !poll_timer_.IsRunning() ||
                       poll != poll_timer_.GetCurrentDelay();

  if (type == FORCE_RESET) {
    last_poll_reset_ = base::TimeTicks::Now();
    if (!rate_changed)
      poll_timer_.Reset();
  }

  if (!rate_changed)
    return;

  // Adjust poll rate.
  poll_timer_.Stop();
  poll_timer_.Start(FROM_HERE, poll, this,
                    &SyncSchedulerImpl::PollTimerCallback);
}

void SyncSchedulerImpl::RestartWaiting() {
  CHECK(wait_interval_.get());
  DCHECK(wait_interval_->length >= TimeDelta::FromSeconds(0));
  NotifyRetryTime(base::Time::Now() + wait_interval_->length);
  SDVLOG(2) << "Starting WaitInterval timer of length "
      << wait_interval_->length.InMilliseconds() << "ms.";
  if (wait_interval_->mode == WaitInterval::THROTTLED) {
    pending_wakeup_timer_.Start(
        FROM_HERE,
        wait_interval_->length,
        base::Bind(&SyncSchedulerImpl::Unthrottle,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    pending_wakeup_timer_.Start(
        FROM_HERE,
        wait_interval_->length,
        base::Bind(&SyncSchedulerImpl::ExponentialBackoffRetry,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void SyncSchedulerImpl::Stop() {
  DCHECK(CalledOnValidThread());
  SDVLOG(2) << "Stop called";

  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  wait_interval_.reset();
  NotifyRetryTime(base::Time());
  poll_timer_.Stop();
  pending_wakeup_timer_.Stop();
  pending_configure_params_.reset();
  if (started_)
    started_ = false;
}

// This is the only place where we invoke DoSyncSessionJob with canary
// privileges.  Everyone else should use NORMAL_PRIORITY.
void SyncSchedulerImpl::TryCanaryJob() {
  next_sync_session_job_priority_ = CANARY_PRIORITY;
  TrySyncSessionJob();
}

void SyncSchedulerImpl::TrySyncSessionJob() {
  // Post call to TrySyncSessionJobImpl on current thread. Later request for
  // access token will be here.
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &SyncSchedulerImpl::TrySyncSessionJobImpl,
      weak_ptr_factory_.GetWeakPtr()));
}

void SyncSchedulerImpl::TrySyncSessionJobImpl() {
  JobPriority priority = next_sync_session_job_priority_;
  next_sync_session_job_priority_ = NORMAL_PRIORITY;

  nudge_tracker_.SetSyncCycleStartTime(base::TimeTicks::Now());

  DCHECK(CalledOnValidThread());
  if (mode_ == CONFIGURATION_MODE) {
    if (pending_configure_params_) {
      SDVLOG(2) << "Found pending configure job";
      DoConfigurationSyncSessionJob(priority);
    }
  } else if (CanRunNudgeJobNow(priority)) {
    if (nudge_tracker_.IsSyncRequired()) {
      SDVLOG(2) << "Found pending nudge job";
      DoNudgeSyncSessionJob(priority);
    } else if (do_poll_after_credentials_updated_ ||
        ((base::TimeTicks::Now() - last_poll_reset_) >= GetPollInterval())) {
      DoPollSyncSessionJob();
      // Poll timer fires infrequently. Usually by this time access token is
      // already expired and poll job will fail with auth error. Set flag to
      // retry poll once ProfileSyncService gets new access token, TryCanaryJob
      // will be called after access token is retrieved.
      if (HttpResponse::SYNC_AUTH_ERROR ==
          session_context_->connection_manager()->server_status()) {
        do_poll_after_credentials_updated_ = true;
      }
    }
  }

  if (priority == CANARY_PRIORITY) {
    // If this is canary job then whatever result was don't run poll job till
    // the next time poll timer fires.
    do_poll_after_credentials_updated_ = false;
  }

  if (IsBackingOff() && !pending_wakeup_timer_.IsRunning()) {
    // If we succeeded, our wait interval would have been cleared.  If it hasn't
    // been cleared, then we should increase our backoff interval and schedule
    // another retry.
    TimeDelta length = delay_provider_->GetDelay(wait_interval_->length);
    wait_interval_.reset(
      new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF, length));
    SDVLOG(2) << "Sync cycle failed.  Will back off for "
        << wait_interval_->length.InMilliseconds() << "ms.";
    RestartWaiting();
  }
}

void SyncSchedulerImpl::PollTimerCallback() {
  DCHECK(CalledOnValidThread());
  if (no_scheduling_allowed_) {
    // The no_scheduling_allowed_ flag is set by a function-scoped AutoReset in
    // functions that are called only on the sync thread.  This function is also
    // called only on the sync thread, and only when it is posted by an expiring
    // timer.  If we find that no_scheduling_allowed_ is set here, then
    // something is very wrong.  Maybe someone mistakenly called us directly, or
    // mishandled the book-keeping for no_scheduling_allowed_.
    NOTREACHED() << "Illegal to schedule job while session in progress.";
    return;
  }

  TrySyncSessionJob();
}

void SyncSchedulerImpl::RetryTimerCallback() {
  TrySyncSessionJob();
}

void SyncSchedulerImpl::Unthrottle() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);

  // We're no longer throttled, so clear the wait interval.
  wait_interval_.reset();
  NotifyRetryTime(base::Time());
  NotifyThrottledTypesChanged(nudge_tracker_.GetThrottledTypes());

  // We treat this as a 'canary' in the sense that it was originally scheduled
  // to run some time ago, failed, and we now want to retry, versus a job that
  // was just created (e.g via ScheduleNudgeImpl). The main implication is
  // that we're careful to update routing info (etc) with such potentially
  // stale canary jobs.
  TryCanaryJob();
}

void SyncSchedulerImpl::TypeUnthrottle(base::TimeTicks unthrottle_time) {
  DCHECK(CalledOnValidThread());
  nudge_tracker_.UpdateTypeThrottlingState(unthrottle_time);
  NotifyThrottledTypesChanged(nudge_tracker_.GetThrottledTypes());

  if (nudge_tracker_.IsAnyTypeThrottled()) {
    const base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta time_until_next_unthrottle =
        nudge_tracker_.GetTimeUntilNextUnthrottle(now);
    type_unthrottle_timer_.Start(
        FROM_HERE,
        time_until_next_unthrottle,
        base::Bind(&SyncSchedulerImpl::TypeUnthrottle,
                   weak_ptr_factory_.GetWeakPtr(),
                   now + time_until_next_unthrottle));
  }

  // Maybe this is a good time to run a nudge job.  Let's try it.
  if (nudge_tracker_.IsSyncRequired() && CanRunNudgeJobNow(NORMAL_PRIORITY))
    TrySyncSessionJob();
}

void SyncSchedulerImpl::PerformDelayedNudge() {
  // Circumstances may have changed since we scheduled this delayed nudge.
  // We must check to see if it's OK to run the job before we do so.
  if (CanRunNudgeJobNow(NORMAL_PRIORITY))
    TrySyncSessionJob();

  // We're not responsible for setting up any retries here.  The functions that
  // first put us into a state that prevents successful sync cycles (eg. global
  // throttling, type throttling, network errors, transient errors) will also
  // setup the appropriate retry logic (eg. retry after timeout, exponential
  // backoff, retry when the network changes).
}

void SyncSchedulerImpl::ExponentialBackoffRetry() {
  TryCanaryJob();
}

void SyncSchedulerImpl::NotifyRetryTime(base::Time retry_time) {
  FOR_EACH_OBSERVER(SyncEngineEventListener,
                    *session_context_->listeners(),
                    OnRetryTimeChanged(retry_time));
}

void SyncSchedulerImpl::NotifyThrottledTypesChanged(ModelTypeSet types) {
  FOR_EACH_OBSERVER(SyncEngineEventListener,
                    *session_context_->listeners(),
                    OnThrottledTypesChanged(types));
}

bool SyncSchedulerImpl::IsBackingOff() const {
  DCHECK(CalledOnValidThread());
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::EXPONENTIAL_BACKOFF;
}

void SyncSchedulerImpl::OnThrottled(const base::TimeDelta& throttle_duration) {
  DCHECK(CalledOnValidThread());
  wait_interval_.reset(new WaitInterval(WaitInterval::THROTTLED,
                                        throttle_duration));
  NotifyRetryTime(base::Time::Now() + wait_interval_->length);
  NotifyThrottledTypesChanged(ModelTypeSet::All());
}

void SyncSchedulerImpl::OnTypesThrottled(
    ModelTypeSet types,
    const base::TimeDelta& throttle_duration) {
  base::TimeTicks now = base::TimeTicks::Now();

  nudge_tracker_.SetTypesThrottledUntil(types, throttle_duration, now);
  base::TimeDelta time_until_next_unthrottle =
      nudge_tracker_.GetTimeUntilNextUnthrottle(now);
  type_unthrottle_timer_.Start(
      FROM_HERE,
      time_until_next_unthrottle,
      base::Bind(&SyncSchedulerImpl::TypeUnthrottle,
                 weak_ptr_factory_.GetWeakPtr(),
                 now + time_until_next_unthrottle));
  NotifyThrottledTypesChanged(nudge_tracker_.GetThrottledTypes());
}

bool SyncSchedulerImpl::IsCurrentlyThrottled() {
  DCHECK(CalledOnValidThread());
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::THROTTLED;
}

void SyncSchedulerImpl::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK(CalledOnValidThread());
  syncer_short_poll_interval_seconds_ = new_interval;
}

void SyncSchedulerImpl::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK(CalledOnValidThread());
  syncer_long_poll_interval_seconds_ = new_interval;
}

void SyncSchedulerImpl::OnReceivedSessionsCommitDelay(
    const base::TimeDelta& new_delay) {
  DCHECK(CalledOnValidThread());
  sessions_commit_delay_ = new_delay;
}

void SyncSchedulerImpl::OnReceivedClientInvalidationHintBufferSize(int size) {
  if (size > 0)
    nudge_tracker_.SetHintBufferSize(size);
  else
    NOTREACHED() << "Hint buffer size should be > 0.";
}

void SyncSchedulerImpl::OnSyncProtocolError(
    const SyncProtocolError& sync_protocol_error) {
  DCHECK(CalledOnValidThread());
  if (ShouldRequestEarlyExit(sync_protocol_error)) {
    SDVLOG(2) << "Sync Scheduler requesting early exit.";
    Stop();
  }
  if (IsActionableError(sync_protocol_error)) {
    SDVLOG(2) << "OnActionableError";
    FOR_EACH_OBSERVER(SyncEngineEventListener,
                      *session_context_->listeners(),
                      OnActionableError(sync_protocol_error));
  }
}

void SyncSchedulerImpl::OnReceivedGuRetryDelay(const base::TimeDelta& delay) {
  nudge_tracker_.SetNextRetryTime(TimeTicks::Now() + delay);
  retry_timer_.Start(FROM_HERE, delay, this,
                     &SyncSchedulerImpl::RetryTimerCallback);
}

void SyncSchedulerImpl::OnReceivedMigrationRequest(ModelTypeSet types) {
    FOR_EACH_OBSERVER(SyncEngineEventListener,
                      *session_context_->listeners(),
                      OnMigrationRequested(types));
}

void SyncSchedulerImpl::SetNotificationsEnabled(bool notifications_enabled) {
  DCHECK(CalledOnValidThread());
  session_context_->set_notifications_enabled(notifications_enabled);
  if (notifications_enabled)
    nudge_tracker_.OnInvalidationsEnabled();
  else
    nudge_tracker_.OnInvalidationsDisabled();
}

base::TimeDelta SyncSchedulerImpl::GetSessionsCommitDelay() const {
  DCHECK(CalledOnValidThread());
  return sessions_commit_delay_;
}

#undef SDVLOG_LOC

#undef SDVLOG

#undef SLOG

#undef ENUM_CASE

}  // namespace syncer
