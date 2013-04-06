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
#include "base/message_loop.h"
#include "sync/engine/backoff_delay_provider.h"
#include "sync/engine/syncer.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/protocol/proto_enum_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/data_type_histogram.h"
#include "sync/util/logging.h"

using base::TimeDelta;
using base::TimeTicks;

namespace syncer {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sessions::SyncSourceInfo;
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
    const base::Closure& ready_task)
    : source(source),
      types_to_download(types_to_download),
      routing_info(routing_info),
      ready_task(ready_task) {
  DCHECK(!ready_task.is_null());
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
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_ptr_factory_for_weak_handle_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_handle_this_(MakeWeakHandle(
          weak_ptr_factory_for_weak_handle_.GetWeakPtr())),
      name_(name),
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
      no_scheduling_allowed_(false) {
}

SyncSchedulerImpl::~SyncSchedulerImpl() {
  DCHECK(CalledOnValidThread());
  StopImpl(base::Closure());
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
  // call DoCanaryJob to achieve this, and note that nothing -- not even a
  // canary job -- can bypass a THROTTLED WaitInterval. The only thing that
  // has the authority to do that is the Unthrottle timer.
  TryCanaryJob();
}

void SyncSchedulerImpl::Start(Mode mode) {
  DCHECK(CalledOnValidThread());
  std::string thread_name = MessageLoop::current()->thread_name();
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
  AdjustPolling(NULL);  // Will kick start poll timer if needed.

  if (old_mode != mode_ && mode_ == NORMAL_MODE && pending_nudge_job_) {
    // We just got back to normal mode.  Let's try to run the work that was
    // queued up while we were configuring.
    DoNudgeSyncSessionJob(NORMAL_PRIORITY);
  }
}

void SyncSchedulerImpl::SendInitialSnapshot() {
  DCHECK(CalledOnValidThread());
  scoped_ptr<SyncSession> dummy(new SyncSession(
          session_context_, this, SyncSourceInfo()));
  SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
  event.snapshot = dummy->TakeSnapshot();
  session_context_->NotifyListeners(event);
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

bool SyncSchedulerImpl::ScheduleConfiguration(
    const ConfigurationParams& params) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsConfigRelatedUpdateSourceValue(params.source));
  DCHECK_EQ(CONFIGURATION_MODE, mode_);
  DCHECK(!params.ready_task.is_null());
  CHECK(started_) << "Scheduler must be running to configure.";
  SDVLOG(2) << "Reconfiguring syncer.";

  // Only one configuration is allowed at a time. Verify we're not waiting
  // for a pending configure job.
  DCHECK(!pending_configure_job_);

  ModelSafeRoutingInfo restricted_routes;
  BuildModelSafeParams(params.types_to_download,
                       params.routing_info,
                       &restricted_routes);
  session_context_->set_routing_info(restricted_routes);

  // Only reconfigure if we have types to download.
  if (!params.types_to_download.Empty()) {
    DCHECK(!restricted_routes.empty());
    pending_configure_job_.reset(new SyncSessionJob(
        SyncSessionJob::CONFIGURATION,
        TimeTicks::Now(),
        SyncSourceInfo(params.source,
                       ModelSafeRoutingInfoToInvalidationMap(
                           restricted_routes,
                           std::string())),
        params));
    bool succeeded = DoConfigurationSyncSessionJob(NORMAL_PRIORITY);

    // If we failed, the job would have been saved as the pending configure
    // job and a wait interval would have been set.
    if (!succeeded) {
      DCHECK(pending_configure_job_);
      return false;
    } else {
      DCHECK(!pending_configure_job_);
    }
  } else {
    SDVLOG(2) << "No change in routing info, calling ready task directly.";
    params.ready_task.Run();
  }

  return true;
}

SyncSchedulerImpl::JobProcessDecision
SyncSchedulerImpl::DecideWhileInWaitInterval(const SyncSessionJob& job,
                                             JobPriority priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(wait_interval_.get());
  DCHECK_NE(job.purpose(), SyncSessionJob::POLL);

  SDVLOG(2) << "DecideWhileInWaitInterval with WaitInterval mode "
            << WaitInterval::GetModeString(wait_interval_->mode)
            << ((priority == CANARY_PRIORITY) ? " (canary)" : "");

  // If we save a job while in a WaitInterval, there is a well-defined moment
  // in time in the future when it makes sense for that SAVE-worthy job to try
  // running again -- the end of the WaitInterval.
  DCHECK(job.purpose() == SyncSessionJob::NUDGE ||
         job.purpose() == SyncSessionJob::CONFIGURATION);

  // If throttled, there's a clock ticking to unthrottle. We want to get
  // on the same train.
  if (wait_interval_->mode == WaitInterval::THROTTLED)
    return SAVE;

  DCHECK_EQ(wait_interval_->mode, WaitInterval::EXPONENTIAL_BACKOFF);
  if (job.purpose() == SyncSessionJob::NUDGE) {
    if (mode_ == CONFIGURATION_MODE)
      return SAVE;

    if (priority == NORMAL_PRIORITY)
      return DROP;
    else // Either backoff has ended, or we have permission to bypass it.
      return CONTINUE;
  }
  return (priority == CANARY_PRIORITY) ? CONTINUE : SAVE;
}

SyncSchedulerImpl::JobProcessDecision SyncSchedulerImpl::DecideOnJob(
    const SyncSessionJob& job,
    JobPriority priority) {
  DCHECK(CalledOnValidThread());

  // POLL jobs do not call this function.
  DCHECK(job.purpose() == SyncSessionJob::NUDGE ||
         job.purpose() == SyncSessionJob::CONFIGURATION);

  // See if our type is throttled.
  ModelTypeSet throttled_types =
      session_context_->throttled_data_type_tracker()->GetThrottledTypes();
  if (job.purpose() == SyncSessionJob::NUDGE &&
      job.source_info().updates_source == GetUpdatesCallerInfo::LOCAL) {
    ModelTypeSet requested_types;
    for (ModelTypeInvalidationMap::const_iterator i =
         job.source_info().types.begin(); i != job.source_info().types.end();
         ++i) {
      requested_types.Put(i->first);
    }

    // If all types are throttled, do not CONTINUE.  Today, we don't treat
    // a per-datatype "unthrottle" event as something that should force a
    // canary job. For this reason, there's no good time to reschedule this job
    // to run -- we'll lazily wait for an independent event to trigger a sync.
    // Note that there may already be such an event if we're in a WaitInterval,
    // so we can retry it then.
    if (!requested_types.Empty() && throttled_types.HasAll(requested_types))
      return DROP;  // TODO(tim): Don't drop. http://crbug.com/177659
  }

  if (wait_interval_.get())
    return DecideWhileInWaitInterval(job, priority);

  if (mode_ == CONFIGURATION_MODE) {
    if (job.purpose() == SyncSessionJob::NUDGE)
      return SAVE;  // Running requires a mode switch.
    else  // Implies job.purpose() == SyncSessionJob::CONFIGURATION.
      return CONTINUE;
  }

  // We are in normal mode.
  DCHECK_EQ(mode_, NORMAL_MODE);
  DCHECK_NE(job.purpose(), SyncSessionJob::CONFIGURATION);

  // Note about some subtle scheduling semantics.
  //
  // It's possible at this point that |job| is known to be unnecessary, and
  // dropping it would be perfectly safe and correct. Consider
  //
  // 1) |job| is a NUDGE (for any combination of types) with a
  //    |scheduled_start| time that is less than the time that the last
  //    successful all-datatype NUDGE completed, and it has a NOTIFICATION
  //    GetUpdatesCallerInfo value yet offers no new notification hint.
  //
  // 2) |job| is a NUDGE with a |scheduled_start| time that is less than
  //    the time that the last successful matching-datatype NUDGE completed,
  //    and payloads (hints) are identical to that last successful NUDGE.
  //
  //  We avoid cases 1 and 2 by externally synchronizing NUDGE requests --
  //  scheduling a NUDGE requires command of the sync thread, which is
  //  impossible* from outside of SyncScheduler if a NUDGE is taking place.
  //  And if you have command of the sync thread when scheduling a NUDGE and a
  //  previous NUDGE exists, they will be coalesced and the stale job will be
  //  cancelled via the session-equality check in DoSyncSessionJob.
  //
  //  * It's not strictly "impossible", but it would be reentrant and hence
  //  illegal. e.g. scheduling a job and re-entering the SyncScheduler is NOT a
  //  legal side effect of any of the work being done as part of a sync cycle.
  //  See |no_scheduling_allowed_| for details.

  // Decision now rests on state of auth tokens.
  if (!session_context_->connection_manager()->HasInvalidAuthToken())
    return CONTINUE;

  SDVLOG(2) << "No valid auth token. Using that to decide on job.";
  // Running the job would require updated auth, so we can't honour
  // job.scheduled_start().
  return job.purpose() == SyncSessionJob::NUDGE ? SAVE : DROP;
}

void SyncSchedulerImpl::ScheduleNudgeAsync(
    const TimeDelta& desired_delay,
    NudgeSource source, ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  SDVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay "
      << desired_delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "types " << ModelTypeSetToString(types);

  ModelTypeInvalidationMap invalidation_map =
      ModelTypeSetToInvalidationMap(types, std::string());
  SyncSchedulerImpl::ScheduleNudgeImpl(desired_delay,
                                       GetUpdatesFromNudgeSource(source),
                                       invalidation_map,
                                       nudge_location);
}

void SyncSchedulerImpl::ScheduleNudgeWithStatesAsync(
    const TimeDelta& desired_delay,
    NudgeSource source, const ModelTypeInvalidationMap& invalidation_map,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  SDVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay "
      << desired_delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "payloads "
      << ModelTypeInvalidationMapToString(invalidation_map);

  SyncSchedulerImpl::ScheduleNudgeImpl(desired_delay,
                                       GetUpdatesFromNudgeSource(source),
                                       invalidation_map,
                                       nudge_location);
}


// TODO(zea): Consider adding separate throttling/backoff for datatype
// refresh requests.
void SyncSchedulerImpl::ScheduleNudgeImpl(
    const TimeDelta& delay,
    GetUpdatesCallerInfo::GetUpdatesSource source,
    const ModelTypeInvalidationMap& invalidation_map,
    const tracked_objects::Location& nudge_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(!invalidation_map.empty()) << "Nudge scheduled for no types!";

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
      << delay.InMilliseconds() << " ms, "
      << "source " << GetUpdatesSourceString(source) << ", "
      << "payloads "
      << ModelTypeInvalidationMapToString(invalidation_map);

  SyncSourceInfo info(source, invalidation_map);
  UpdateNudgeTimeRecords(info);

  scoped_ptr<SyncSessionJob> job(new SyncSessionJob(
      SyncSessionJob::NUDGE,
      TimeTicks::Now() + delay,
      info,
      ConfigurationParams()));
  JobProcessDecision decision = DecideOnJob(*job, NORMAL_PRIORITY);
  SDVLOG(2) << "Should run "
            << SyncSessionJob::GetPurposeString(job->purpose())
            << " in mode " << GetModeString(mode_)
            << ": " << GetDecisionString(decision);
  if (decision == DROP) {
    return;
  }

  // Try to coalesce in both SAVE and CONTINUE cases.
  if (pending_nudge_job_) {
    pending_nudge_job_->CoalesceSources(job->source_info());
    if (decision == CONTINUE) {
      // Only update the scheduled_start if we're going to reschedule.
      pending_nudge_job_->set_scheduled_start(
          std::min(job->scheduled_start(),
                   pending_nudge_job_->scheduled_start()));
    }
  } else {
    pending_nudge_job_ = job.Pass();
  }

  if (decision == SAVE) {
    return;
  }

  TimeDelta run_delay =
      pending_nudge_job_->scheduled_start() - TimeTicks::Now();
  if (run_delay < TimeDelta::FromMilliseconds(0))
    run_delay = TimeDelta::FromMilliseconds(0);
  SDVLOG_LOC(nudge_location, 2)
      << "Scheduling a nudge with "
      << run_delay.InMilliseconds() << " ms delay";

  if (started_) {
    pending_wakeup_timer_.Start(
        nudge_location,
        run_delay,
        base::Bind(&SyncSchedulerImpl::DoNudgeSyncSessionJob,
                   weak_ptr_factory_.GetWeakPtr(),
                   NORMAL_PRIORITY));
  }
}

const char* SyncSchedulerImpl::GetModeString(SyncScheduler::Mode mode) {
  switch (mode) {
    ENUM_CASE(CONFIGURATION_MODE);
    ENUM_CASE(NORMAL_MODE);
  }
  return "";
}

const char* SyncSchedulerImpl::GetDecisionString(
    SyncSchedulerImpl::JobProcessDecision mode) {
  switch (mode) {
    ENUM_CASE(CONTINUE);
    ENUM_CASE(SAVE);
    ENUM_CASE(DROP);
  }
  return "";
}

bool SyncSchedulerImpl::DoSyncSessionJobImpl(scoped_ptr<SyncSessionJob> job,
                                             JobPriority priority) {
  DCHECK(CalledOnValidThread());

  base::AutoReset<bool> protector(&no_scheduling_allowed_, true);
  JobProcessDecision decision = DecideOnJob(*job, priority);
  SDVLOG(2) << "Should run "
            << SyncSessionJob::GetPurposeString(job->purpose())
            << " in mode " << GetModeString(mode_)
            << " with source " << job->source_info().updates_source
            << ": " << GetDecisionString(decision);
  if (decision != CONTINUE) {
    if (decision == SAVE) {
      if (job->purpose() == SyncSessionJob::CONFIGURATION) {
        pending_configure_job_ = job.Pass();
      } else {
        pending_nudge_job_ = job.Pass();
      }
    } else {
      DCHECK_EQ(decision, DROP);
    }
    return false;
  }

  DVLOG(2) << "Creating sync session with routes "
           << ModelSafeRoutingInfoToString(session_context_->routing_info())
           << "and purpose " << job->purpose();
  SyncSession session(session_context_, this, job->source_info());
  bool premature_exit = !syncer_->SyncShare(&session,
                                            job->start_step(),
                                            job->end_step());
  SDVLOG(2) << "Done SyncShare, returned: " << premature_exit;

  bool success = FinishSyncSessionJob(job.get(),
                                      premature_exit,
                                      &session);

  if (IsSyncingCurrentlySilenced()) {
    SDVLOG(2) << "We are currently throttled; scheduling Unthrottle.";
    // If we're here, it's because |job| was silenced until a server specified
    // time. (Note, it had to be |job|, because DecideOnJob would not permit
    // any job through while in WaitInterval::THROTTLED).
    if (job->purpose() == SyncSessionJob::NUDGE)
      pending_nudge_job_ = job.Pass();
    else if (job->purpose() == SyncSessionJob::CONFIGURATION)
      pending_configure_job_ = job.Pass();
    else
      NOTREACHED();

    RestartWaiting();
    return success;
  }

  if (!success)
    ScheduleNextSync(job.Pass(), &session);

  return success;
}

void SyncSchedulerImpl::DoNudgeSyncSessionJob(JobPriority priority) {
  DoSyncSessionJobImpl(pending_nudge_job_.Pass(), priority);
}

bool SyncSchedulerImpl::DoConfigurationSyncSessionJob(JobPriority priority) {
  return DoSyncSessionJobImpl(pending_configure_job_.Pass(), priority);
}

bool SyncSchedulerImpl::ShouldPoll() {
  if (wait_interval_.get()) {
    SDVLOG(2) << "Not running poll in wait interval.";
    return false;
  }

  if (mode_ == CONFIGURATION_MODE) {
    SDVLOG(2) << "Not running poll in configuration mode.";
    return false;
  }

  // TODO(rlarocque): Refactor decision-making logic common to all types
  // of jobs into a shared function.

  if (session_context_->connection_manager()->HasInvalidAuthToken()) {
    SDVLOG(2) << "Not running poll because auth token is invalid.";
    return false;
  }

  return true;
}

void SyncSchedulerImpl::DoPollSyncSessionJob() {
  ModelSafeRoutingInfo r;
  ModelTypeInvalidationMap invalidation_map =
      ModelSafeRoutingInfoToInvalidationMap(r, std::string());
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, invalidation_map);
  scoped_ptr<SyncSessionJob> job(new SyncSessionJob(SyncSessionJob::POLL,
                                                    TimeTicks::Now(),
                                                    info,
                                                    ConfigurationParams()));

  base::AutoReset<bool> protector(&no_scheduling_allowed_, true);

  if (!ShouldPoll())
    return;

  DVLOG(2) << "Polling with routes "
           << ModelSafeRoutingInfoToString(session_context_->routing_info());
  SyncSession session(session_context_, this, job->source_info());
  bool premature_exit = !syncer_->SyncShare(&session,
                                            job->start_step(),
                                            job->end_step());
  SDVLOG(2) << "Done SyncShare, returned: " << premature_exit;

  FinishSyncSessionJob(job.get(), premature_exit, &session);

  if (IsSyncingCurrentlySilenced()) {
    // Normally we would only call RestartWaiting() if we had a
    // pending_nudge_job_ or pending_configure_job_ set.  In this case, it's
    // possible that neither is set.  We create the wait interval anyway because
    // we need it to make sure we get unthrottled on time.
    RestartWaiting();
  }
}

void SyncSchedulerImpl::UpdateNudgeTimeRecords(const SyncSourceInfo& info) {
  DCHECK(CalledOnValidThread());

  // We are interested in recording time between local nudges for datatypes.
  // TODO(tim): Consider tracking LOCAL_NOTIFICATION as well.
  if (info.updates_source != GetUpdatesCallerInfo::LOCAL)
    return;

  base::TimeTicks now = TimeTicks::Now();
  // Update timing information for how often datatypes are triggering nudges.
  for (ModelTypeInvalidationMap::const_iterator iter = info.types.begin();
       iter != info.types.end();
       ++iter) {
    base::TimeTicks previous = last_local_nudges_by_model_type_[iter->first];
    last_local_nudges_by_model_type_[iter->first] = now;
    if (previous.is_null())
      continue;

#define PER_DATA_TYPE_MACRO(type_str) \
    SYNC_FREQ_HISTOGRAM("Sync.Freq" type_str, now - previous);
    SYNC_DATA_TYPE_HISTOGRAM(iter->first);
#undef PER_DATA_TYPE_MACRO
  }
}

bool SyncSchedulerImpl::FinishSyncSessionJob(SyncSessionJob* job,
                                             bool exited_prematurely,
                                             SyncSession* session) {
  DCHECK(CalledOnValidThread());

  // Let job know that we're through syncing (calling SyncShare) at this point.
  bool succeeded = false;
  {
    base::AutoReset<bool> protector(&no_scheduling_allowed_, true);
    succeeded = job->Finish(exited_prematurely, session);
  }

  SDVLOG(2) << "Updating the next polling time after SyncMain";

  AdjustPolling(job);

  if (succeeded) {
    // No job currently supported by the scheduler could succeed without
    // successfully reaching the server.  Therefore, if we make it here, it is
    // appropriate to reset the backoff interval.
    wait_interval_.reset();
    NotifyRetryTime(base::Time());
    SDVLOG(2) << "Job succeeded so not scheduling more jobs";
  }

  return succeeded;
}

void SyncSchedulerImpl::ScheduleNextSync(
    scoped_ptr<SyncSessionJob> finished_job,
    SyncSession* session) {
  DCHECK(CalledOnValidThread());
  DCHECK(finished_job->purpose() == SyncSessionJob::CONFIGURATION
         || finished_job->purpose() == SyncSessionJob::NUDGE);

  // TODO(rlarocque): There's no reason why we should blindly backoff and retry
  // if we don't succeed.  Some types of errors are not likely to disappear on
  // their own.  With the return values now available in the old_job.session,
  // we should be able to detect such errors and only retry when we detect
  // transient errors.

  SDVLOG(2) << "SyncShare job failed; will start or update backoff";
  HandleContinuationError(finished_job.Pass(), session);
}

void SyncSchedulerImpl::AdjustPolling(const SyncSessionJob* old_job) {
  DCHECK(CalledOnValidThread());

  TimeDelta poll  = (!session_context_->notifications_enabled()) ?
      syncer_short_poll_interval_seconds_ :
      syncer_long_poll_interval_seconds_;
  bool rate_changed = !poll_timer_.IsRunning() ||
                       poll != poll_timer_.GetCurrentDelay();

  if (old_job && old_job->purpose() != SyncSessionJob::POLL && !rate_changed)
    poll_timer_.Reset();

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
        base::Bind(&SyncSchedulerImpl::TryCanaryJob,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void SyncSchedulerImpl::HandleContinuationError(
    scoped_ptr<SyncSessionJob> old_job,
    SyncSession* session) {
  DCHECK(CalledOnValidThread());

  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length :
          delay_provider_->GetInitialDelay(
              session->status_controller().model_neutral_state()));

  SDVLOG(2) << "In handle continuation error with "
            << SyncSessionJob::GetPurposeString(old_job->purpose())
            << " job. The time delta(ms) is "
            << length.InMilliseconds();

  wait_interval_.reset(new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF,
                                        length));
  NotifyRetryTime(base::Time::Now() + length);
  old_job->set_scheduled_start(TimeTicks::Now() + length);
  if (old_job->purpose() == SyncSessionJob::CONFIGURATION) {
    SDVLOG(2) << "Configuration did not succeed, scheduling retry.";
    // Config params should always get set.
    DCHECK(!old_job->config_params().ready_task.is_null());
    DCHECK(!pending_configure_job_);
    pending_configure_job_ = old_job.Pass();
  } else {
    // We're not in configure mode so we should not have a configure job.
    DCHECK(!pending_configure_job_);
    DCHECK(!pending_nudge_job_);
    pending_nudge_job_ = old_job.Pass();
  }

  RestartWaiting();
}

void SyncSchedulerImpl::RequestStop(const base::Closure& callback) {
  syncer_->RequestEarlyExit();  // Safe to call from any thread.
  DCHECK(weak_handle_this_.IsInitialized());
  SDVLOG(3) << "Posting StopImpl";
  weak_handle_this_.Call(FROM_HERE,
                         &SyncSchedulerImpl::StopImpl,
                         callback);
}

void SyncSchedulerImpl::StopImpl(const base::Closure& callback) {
  DCHECK(CalledOnValidThread());
  SDVLOG(2) << "StopImpl called";

  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  wait_interval_.reset();
  NotifyRetryTime(base::Time());
  poll_timer_.Stop();
  pending_wakeup_timer_.Stop();
  pending_nudge_job_.reset();
  pending_configure_job_.reset();
  if (started_) {
    started_ = false;
  }
  if (!callback.is_null())
    callback.Run();
}

// This is the only place where we invoke DoSyncSessionJob with canary
// privileges.  Everyone else should use NORMAL_PRIORITY.
void SyncSchedulerImpl::TryCanaryJob() {
  DCHECK(CalledOnValidThread());

  if (mode_ == CONFIGURATION_MODE && pending_configure_job_) {
    SDVLOG(2) << "Found pending configure job; will run as canary";
    DoConfigurationSyncSessionJob(CANARY_PRIORITY);
  } else if (mode_ == NORMAL_MODE && pending_nudge_job_) {
    SDVLOG(2) << "Found pending nudge job; will run as canary";
    DoNudgeSyncSessionJob(CANARY_PRIORITY);
  } else {
    SDVLOG(2) << "Found no work to do; will not run a canary";
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

  DoPollSyncSessionJob();
}

void SyncSchedulerImpl::Unthrottle() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);

  // We're no longer throttled, so clear the wait interval.
  wait_interval_.reset();
  NotifyRetryTime(base::Time());

  // We treat this as a 'canary' in the sense that it was originally scheduled
  // to run some time ago, failed, and we now want to retry, versus a job that
  // was just created (e.g via ScheduleNudgeImpl). The main implication is
  // that we're careful to update routing info (etc) with such potentially
  // stale canary jobs.
  TryCanaryJob();
}

void SyncSchedulerImpl::Notify(SyncEngineEvent::EventCause cause) {
  DCHECK(CalledOnValidThread());
  session_context_->NotifyListeners(SyncEngineEvent(cause));
}

void SyncSchedulerImpl::NotifyRetryTime(base::Time retry_time) {
  SyncEngineEvent event(SyncEngineEvent::RETRY_TIME_CHANGED);
  event.retry_time = retry_time;
  session_context_->NotifyListeners(event);
}

bool SyncSchedulerImpl::IsBackingOff() const {
  DCHECK(CalledOnValidThread());
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::EXPONENTIAL_BACKOFF;
}

void SyncSchedulerImpl::OnSilencedUntil(
    const base::TimeTicks& silenced_until) {
  DCHECK(CalledOnValidThread());
  wait_interval_.reset(new WaitInterval(WaitInterval::THROTTLED,
                                        silenced_until - TimeTicks::Now()));
  NotifyRetryTime(base::Time::Now() + wait_interval_->length);
}

bool SyncSchedulerImpl::IsSyncingCurrentlySilenced() {
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

void SyncSchedulerImpl::OnShouldStopSyncingPermanently() {
  DCHECK(CalledOnValidThread());
  SDVLOG(2) << "OnShouldStopSyncingPermanently";
  syncer_->RequestEarlyExit();  // Thread-safe.
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncSchedulerImpl::OnActionableError(
    const sessions::SyncSessionSnapshot& snap) {
  DCHECK(CalledOnValidThread());
  SDVLOG(2) << "OnActionableError";
  SyncEngineEvent event(SyncEngineEvent::ACTIONABLE_ERROR);
  event.snapshot = snap;
  session_context_->NotifyListeners(event);
}

void SyncSchedulerImpl::OnSyncProtocolError(
    const sessions::SyncSessionSnapshot& snapshot) {
  DCHECK(CalledOnValidThread());
  if (ShouldRequestEarlyExit(
          snapshot.model_neutral_state().sync_protocol_error)) {
    SDVLOG(2) << "Sync Scheduler requesting early exit.";
    syncer_->RequestEarlyExit();  // Thread-safe.
  }
  if (IsActionableError(snapshot.model_neutral_state().sync_protocol_error))
    OnActionableError(snapshot);
}

void SyncSchedulerImpl::SetNotificationsEnabled(bool notifications_enabled) {
  DCHECK(CalledOnValidThread());
  session_context_->set_notifications_enabled(notifications_enabled);
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
