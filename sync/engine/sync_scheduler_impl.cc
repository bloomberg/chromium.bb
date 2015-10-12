// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_scheduler_impl.h"

#include <algorithm>
#include <cstring>

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

bool IsConfigRelatedUpdateSourceValue(
    GetUpdatesCallerInfo::GetUpdatesSource source) {
  switch (source) {
    case GetUpdatesCallerInfo::RECONFIGURATION:
    case GetUpdatesCallerInfo::MIGRATION:
    case GetUpdatesCallerInfo::NEW_CLIENT:
    case GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE:
    case GetUpdatesCallerInfo::PROGRAMMATIC:
      return true;
    default:
      return false;
  }
}

bool ShouldRequestEarlyExit(const SyncProtocolError& error) {
  switch (error.error_type) {
    case SYNC_SUCCESS:
    case MIGRATION_DONE:
    case THROTTLED:
    case TRANSIENT_ERROR:
      return false;
    case NOT_MY_BIRTHDAY:
    case CLIENT_DATA_OBSOLETE:
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

void RunAndReset(base::Closure* task) {
  DCHECK(task);
  if (task->is_null())
    return;
  task->Run();
  task->Reset();
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
}
ConfigurationParams::~ConfigurationParams() {}

ClearParams::ClearParams(const base::Closure& report_success_task)
    : report_success_task(report_success_task) {
  DCHECK(!report_success_task.is_null());
}
ClearParams::~ClearParams() {}

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
      mode_(CONFIGURATION_MODE),
      delay_provider_(delay_provider),
      syncer_(syncer),
      session_context_(context),
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

void SyncSchedulerImpl::Start(Mode mode, base::Time last_poll_time) {
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

  if (mode == CLEAR_SERVER_DATA_MODE) {
    DCHECK_EQ(mode_, CONFIGURATION_MODE);
  }
  Mode old_mode = mode_;
  mode_ = mode;
  // Only adjust the poll reset time if it was valid and in the past.
  if (!last_poll_time.is_null() && last_poll_time < base::Time::Now()) {
    // Convert from base::Time to base::TimeTicks. The reason we use Time
    // for persisting is that TimeTicks can stop making forward progress when
    // the machine is suspended. This implies that on resume the client might
    // actually have miss the real poll, unless the client is restarted. Fixing
    // that would require using an AlarmTimer though, which is only supported
    // on certain platforms.
    last_poll_reset_ =
        base::TimeTicks::Now() - (base::Time::Now() - last_poll_time);
  }

  if (old_mode != mode_ && mode_ == NORMAL_MODE) {
    // We just got back to normal mode.  Let's try to run the work that was
    // queued up while we were configuring.

    AdjustPolling(UPDATE_INTERVAL);  // Will kick start poll timer if needed.

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

void SyncSchedulerImpl::ScheduleClearServerData(const ClearParams& params) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(CLEAR_SERVER_DATA_MODE, mode_);
  DCHECK(!pending_configure_params_);
  DCHECK(!params.report_success_task.is_null());
  CHECK(started_) << "Scheduler must be running to clear.";
  pending_clear_params_.reset(new ClearParams(params));
  TrySyncSessionJob();
}

bool SyncSchedulerImpl::CanRunJobNow(JobPriority priority) {
  DCHECK(CalledOnValidThread());
  if (IsCurrentlyThrottled()) {
    SDVLOG(1) << "Unable to run a job because we're throttled.";
    return false;
  }

  if (IsBackingOff() && priority != CANARY_PRIORITY) {
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

  if (mode_ != NORMAL_MODE) {
    SDVLOG(1) << "Not running nudge because we're not in normal mode.";
    return false;
  }

  return true;
}

void SyncSchedulerImpl::ScheduleLocalNudge(
    ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(!types.Empty());

  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling sync because of local change to "
      << ModelTypeSetToString(types);
  UpdateNudgeTimeRecords(types);
  base::TimeDelta nudge_delay = nudge_tracker_.RecordLocalChange(types);
  ScheduleNudgeImpl(nudge_delay, nudge_location);
}

void SyncSchedulerImpl::ScheduleLocalRefreshRequest(
    ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(!types.Empty());

  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling sync because of local refresh request for "
      << ModelTypeSetToString(types);
  base::TimeDelta nudge_delay = nudge_tracker_.RecordLocalRefreshRequest(types);
  ScheduleNudgeImpl(nudge_delay, nudge_location);
}

void SyncSchedulerImpl::ScheduleInvalidationNudge(
    syncer::ModelType model_type,
    scoped_ptr<InvalidationInterface> invalidation,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());

  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling sync because we received invalidation for "
      << ModelTypeToString(model_type);
  base::TimeDelta nudge_delay =
      nudge_tracker_.RecordRemoteInvalidation(model_type, invalidation.Pass());
  ScheduleNudgeImpl(nudge_delay, nudge_location);
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
  CHECK(!syncer_->IsSyncing());

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
    ENUM_CASE(CLEAR_SERVER_DATA_MODE);
    ENUM_CASE(NORMAL_MODE);
  }
  return "";
}

void SyncSchedulerImpl::SetDefaultNudgeDelay(base::TimeDelta delay_ms) {
  DCHECK(CalledOnValidThread());
  nudge_tracker_.SetDefaultNudgeDelay(delay_ms);
}

void SyncSchedulerImpl::DoNudgeSyncSessionJob(JobPriority priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(CanRunNudgeJobNow(priority));

  DVLOG(2) << "Will run normal mode sync cycle with types "
           << ModelTypeSetToString(session_context_->GetEnabledTypes());
  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  bool success = syncer_->NormalSyncShare(
      GetEnabledAndUnthrottledTypes(), &nudge_tracker_, session.get());

  if (success) {
    // That cycle took care of any outstanding work we had.
    SDVLOG(2) << "Nudge succeeded.";
    nudge_tracker_.RecordSuccessfulSyncCycle();
    scheduled_nudge_time_ = base::TimeTicks();
    HandleSuccess();

    // If this was a canary, we may need to restart the poll timer (the poll
    // timer may have fired while the scheduler was in an error state, ignoring
    // the poll).
    if (!poll_timer_.IsRunning()) {
      SDVLOG(1) << "Canary succeeded, restarting polling.";
      AdjustPolling(UPDATE_INTERVAL);
    }
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
    RunAndReset(&pending_configure_params_->retry_task);
    return;
  }

  SDVLOG(2) << "Will run configure SyncShare with types "
            << ModelTypeSetToString(session_context_->GetEnabledTypes());
  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  bool success = syncer_->ConfigureSyncShare(
      pending_configure_params_->types_to_download,
      pending_configure_params_->source,
      session.get());

  if (success) {
    SDVLOG(2) << "Configure succeeded.";
    pending_configure_params_->ready_task.Run();
    pending_configure_params_.reset();
    HandleSuccess();
  } else {
    HandleFailure(session->status_controller().model_neutral_state());
    // Sync cycle might receive response from server that causes scheduler to
    // stop and draws pending_configure_params_ invalid.
    if (started_)
      RunAndReset(&pending_configure_params_->retry_task);
  }
}

void SyncSchedulerImpl::DoClearServerDataSyncSessionJob(JobPriority priority) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(mode_, CLEAR_SERVER_DATA_MODE);

  if (!CanRunJobNow(priority)) {
    SDVLOG(2) << "Unable to run clear server data job right now.";
    RunAndReset(&pending_configure_params_->retry_task);
    return;
  }

  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  const bool success = syncer_->PostClearServerData(session.get());
  if (!success) {
    HandleFailure(session->status_controller().model_neutral_state());
    return;
  }

  SDVLOG(2) << "Clear succeeded.";
  pending_clear_params_->report_success_task.Run();
  pending_clear_params_.reset();
  HandleSuccess();
}

void SyncSchedulerImpl::HandleSuccess() {
  // If we're here, then we successfully reached the server.  End all backoff.
  wait_interval_.reset();
  NotifyRetryTime(base::Time());
}

void SyncSchedulerImpl::HandleFailure(
    const sessions::ModelNeutralState& model_neutral_state) {
  if (IsCurrentlyThrottled()) {
    SDVLOG(2) << "Was throttled during previous sync cycle.";
  } else if (!IsBackingOff()) {
    // Setup our backoff if this is our first such failure.
    TimeDelta length = delay_provider_->GetDelay(
        delay_provider_->GetInitialDelay(model_neutral_state));
    wait_interval_.reset(
        new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF, length));
    SDVLOG(2) << "Sync cycle failed.  Will back off for "
        << wait_interval_->length.InMilliseconds() << "ms.";
  } else {
    // Increase our backoff interval and schedule another retry.
    TimeDelta length = delay_provider_->GetDelay(wait_interval_->length);
    wait_interval_.reset(
        new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF, length));
    SDVLOG(2) << "Sync cycle failed.  Will back off for "
        << wait_interval_->length.InMilliseconds() << "ms.";
  }
  RestartWaiting();
}

void SyncSchedulerImpl::DoPollSyncSessionJob() {
  SDVLOG(2) << "Polling with types "
            << ModelTypeSetToString(GetEnabledAndUnthrottledTypes());
  scoped_ptr<SyncSession> session(SyncSession::Build(session_context_, this));
  bool success = syncer_->PollSyncShare(
      GetEnabledAndUnthrottledTypes(),
      session.get());

  // Only restart the timer if the poll succeeded. Otherwise rely on normal
  // failure handling to retry with backoff.
  if (success) {
    AdjustPolling(FORCE_RESET);
    HandleSuccess();
  } else {
    HandleFailure(session->status_controller().model_neutral_state());
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
  if (!started_)
    return;

  TimeDelta poll_interval = GetPollInterval();
  TimeDelta poll_delay = poll_interval;
  const TimeTicks now = TimeTicks::Now();

  if (type == UPDATE_INTERVAL) {
    if (!last_poll_reset_.is_null()) {
      // Override the delay based on the last successful poll time (if it was
      // set).
      TimeTicks new_poll_time = poll_interval + last_poll_reset_;
      poll_delay = new_poll_time - TimeTicks::Now();

      if (poll_delay < TimeDelta()) {
        // The desired poll time was in the past, so trigger a poll now (the
        // timer will post the task asynchronously, so re-entrancy isn't an
        // issue).
        poll_delay = TimeDelta();
      }
    } else {
      // There was no previous poll. Keep the delay set to the normal interval,
      // as if we had just completed a poll.
      DCHECK_EQ(GetPollInterval(), poll_delay);
      last_poll_reset_ = now;
    }
  } else {
    // Otherwise just restart the timer.
    DCHECK_EQ(FORCE_RESET, type);
    DCHECK_EQ(GetPollInterval(), poll_delay);
    last_poll_reset_ = now;
  }

  SDVLOG(1) << "Updating polling delay to " << poll_delay.InMinutes()
            << " minutes.";

  // Adjust poll rate. Start will reset the timer if it was already running.
  poll_timer_.Start(FROM_HERE, poll_delay, this,
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
  pending_clear_params_.reset();
  if (started_)
    started_ = false;
}

// This is the only place where we invoke DoSyncSessionJob with canary
// privileges.  Everyone else should use NORMAL_PRIORITY.
void SyncSchedulerImpl::TryCanaryJob() {
  next_sync_session_job_priority_ = CANARY_PRIORITY;
  SDVLOG(2) << "Attempting canary job";
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
  } else if (mode_ == CLEAR_SERVER_DATA_MODE) {
    if (pending_clear_params_) {
      DoClearServerDataSyncSessionJob(priority);
    }
  } else if (CanRunNudgeJobNow(priority)) {
    if (nudge_tracker_.IsSyncRequired()) {
      SDVLOG(2) << "Found pending nudge job";
      DoNudgeSyncSessionJob(priority);
    } else if (((base::TimeTicks::Now() - last_poll_reset_) >=
                GetPollInterval())) {
      SDVLOG(2) << "Found pending poll";
      DoPollSyncSessionJob();
    }
  } else {
    // We must be in an error state. Transitioning out of each of these
    // error states should trigger a canary job.
    DCHECK(IsCurrentlyThrottled() || IsBackingOff() ||
           session_context_->connection_manager()->HasInvalidAuthToken());
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
  CHECK(!syncer_->IsSyncing());

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

  SDVLOG(1) << "Throttling " << ModelTypeSetToString(types) << " for "
            << throttle_duration.InMinutes() << " minutes.";

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
  if (new_interval == syncer_short_poll_interval_seconds_)
    return;
  SDVLOG(1) << "Updating short poll interval to " << new_interval.InMinutes()
            << " minutes.";
  syncer_short_poll_interval_seconds_ = new_interval;
  AdjustPolling(UPDATE_INTERVAL);
}

void SyncSchedulerImpl::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK(CalledOnValidThread());
  if (new_interval == syncer_long_poll_interval_seconds_)
    return;
  SDVLOG(1) << "Updating long poll interval to " << new_interval.InMinutes()
            << " minutes.";
  syncer_long_poll_interval_seconds_ = new_interval;
  AdjustPolling(UPDATE_INTERVAL);
}

void SyncSchedulerImpl::OnReceivedCustomNudgeDelays(
    const std::map<ModelType, base::TimeDelta>& nudge_delays) {
  DCHECK(CalledOnValidThread());
  nudge_tracker_.OnReceivedCustomNudgeDelays(nudge_delays);
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

#undef SDVLOG_LOC

#undef SDVLOG

#undef SLOG

#undef ENUM_CASE

}  // namespace syncer
