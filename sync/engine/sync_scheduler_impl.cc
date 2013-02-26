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
    : mode(UNKNOWN),
      had_nudge(false),
      pending_configure_job(NULL) {}

SyncSchedulerImpl::WaitInterval::WaitInterval(Mode mode, TimeDelta length)
    : mode(mode), had_nudge(false), length(length),
      pending_configure_job(NULL) {}

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
      sync_loop_(MessageLoop::current()),
      started_(false),
      syncer_short_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds)),
      syncer_long_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultLongPollIntervalSeconds)),
      sessions_commit_delay_(
          TimeDelta::FromSeconds(kDefaultSessionsCommitDelaySeconds)),
      mode_(NORMAL_MODE),
      // Start with assuming everything is fine with the connection.
      // At the end of the sync cycle we would have the correct status.
      connection_code_(HttpResponse::SERVER_CONNECTION_OK),
      pending_nudge_(NULL),
      delay_provider_(delay_provider),
      syncer_(syncer),
      session_context_(context),
      no_scheduling_allowed_(false) {
  DCHECK(sync_loop_);
}

SyncSchedulerImpl::~SyncSchedulerImpl() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  StopImpl(base::Closure());
}

void SyncSchedulerImpl::OnJobDestroyed(SyncSessionJob* job) {
  // TODO(tim): Bug 165561 investigation.
  CHECK(!pending_nudge_ || pending_nudge_ != job);
}

void SyncSchedulerImpl::OnCredentialsUpdated() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  // TODO(lipalani): crbug.com/106262. One issue here is that if after
  // the auth error we happened to do gettime and it succeeded then
  // the |connection_code_| would be briefly OK however it would revert
  // back to SYNC_AUTH_ERROR at the end of the sync cycle. The
  // referenced bug explores the option of removing gettime calls
  // altogethere
  // TODO(rogerta): this code no longer checks |connection_code_|.  It uses
  // ServerConnectionManager::server_status() instead.  This is to resolve a
  // missing notitification during re-auth.  |connection_code_| is a duplicate
  // value and should probably be removed, see comment in the function
  // SyncSchedulerImpl::FinishSyncSessionJob() below.
  if (HttpResponse::SYNC_AUTH_ERROR ==
      session_context_->connection_manager()->server_status()) {
    OnServerConnectionErrorFixed();
  }
}

void SyncSchedulerImpl::OnConnectionStatusChange() {
  if (HttpResponse::CONNECTION_UNAVAILABLE  == connection_code_) {
    // Optimistically assume that the connection is fixed and try
    // connecting.
    OnServerConnectionErrorFixed();
  }
}

void SyncSchedulerImpl::OnServerConnectionErrorFixed() {
  connection_code_ = HttpResponse::SERVER_CONNECTION_OK;
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
  scoped_ptr<SyncSessionJob> pending(TakePendingJobForCurrentMode());
  if (!pending.get())
    return;

  PostTask(FROM_HERE, "DoCanaryJob",
           base::Bind(&SyncSchedulerImpl::DoCanaryJob,
                      weak_ptr_factory_.GetWeakPtr(),
                      base::Passed(&pending)));
}

void SyncSchedulerImpl::UpdateServerConnectionManagerStatus(
    HttpResponse::ServerConnectionCode code) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "New server connection code: "
            << HttpResponse::GetServerConnectionCodeString(code);

  connection_code_ = code;
}

void SyncSchedulerImpl::Start(Mode mode) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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

  if (old_mode != mode_) {
    // We just changed our mode. See if there are any pending jobs that we could
    // execute in the new mode.
    if (mode_ == NORMAL_MODE) {
      // It is illegal to switch to NORMAL_MODE if a previous CONFIGURATION job
      // has not yet completed.
      DCHECK(!wait_interval_.get() || !wait_interval_->pending_configure_job);
    }

    scoped_ptr<SyncSessionJob> pending(TakePendingJobForCurrentMode());
    if (pending.get()) {
      SDVLOG(2) << "Executing pending job. Good luck!";
      DoSyncSessionJob(pending.Pass());
    }
  }
}

void SyncSchedulerImpl::SendInitialSnapshot() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(IsConfigRelatedUpdateSourceValue(params.source));
  DCHECK_EQ(CONFIGURATION_MODE, mode_);
  DCHECK(!params.ready_task.is_null());
  CHECK(started_) << "Scheduler must be running to configure.";
  SDVLOG(2) << "Reconfiguring syncer.";

  // Only one configuration is allowed at a time. Verify we're not waiting
  // for a pending configure job.
  DCHECK(!wait_interval_.get() || !wait_interval_->pending_configure_job);

  ModelSafeRoutingInfo restricted_routes;
  BuildModelSafeParams(params.types_to_download,
                       params.routing_info,
                       &restricted_routes);
  session_context_->set_routing_info(restricted_routes);

  // Only reconfigure if we have types to download.
  if (!params.types_to_download.Empty()) {
    DCHECK(!restricted_routes.empty());
    scoped_ptr<SyncSession> session(new SyncSession(
        session_context_,
        this,
        SyncSourceInfo(params.source,
                       ModelSafeRoutingInfoToInvalidationMap(
                           restricted_routes,
                           std::string()))));
    scoped_ptr<SyncSessionJob> job(new SyncSessionJob(
        SyncSessionJob::CONFIGURATION,
        TimeTicks::Now(),
        session.Pass(),
        params,
        FROM_HERE));
    job->set_destruction_observer(weak_ptr_factory_.GetWeakPtr());
    bool succeeded = DoSyncSessionJob(job.Pass());

    // If we failed, the job would have been saved as the pending configure
    // job and a wait interval would have been set.
    if (!succeeded) {
      DCHECK(wait_interval_.get() && wait_interval_->pending_configure_job);
      return false;
    }
  } else {
    SDVLOG(2) << "No change in routing info, calling ready task directly.";
    params.ready_task.Run();
  }

  return true;
}

SyncSchedulerImpl::JobProcessDecision
SyncSchedulerImpl::DecideWhileInWaitInterval(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(wait_interval_.get());

  SDVLOG(2) << "DecideWhileInWaitInterval with WaitInterval mode "
            << WaitInterval::GetModeString(wait_interval_->mode)
            << (wait_interval_->had_nudge ? " (had nudge)" : "")
            << (job.is_canary() ? " (canary)" : "");

  if (job.purpose() == SyncSessionJob::POLL)
    return DROP;

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

    // If we already had one nudge then just drop this nudge. We will retry
    // later when the timer runs out.
    if (!job.is_canary())
      return wait_interval_->had_nudge ? DROP : CONTINUE;
    else // We are here because timer ran out. So retry.
      return CONTINUE;
  }
  return job.is_canary() ? CONTINUE : SAVE;
}

SyncSchedulerImpl::JobProcessDecision SyncSchedulerImpl::DecideOnJob(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  // See if our type is throttled.
  ModelTypeSet throttled_types =
      session_context_->throttled_data_type_tracker()->GetThrottledTypes();
  if (job.purpose() == SyncSessionJob::NUDGE &&
      job.session()->source().updates_source == GetUpdatesCallerInfo::LOCAL) {
    ModelTypeSet requested_types;
    for (ModelTypeInvalidationMap::const_iterator i =
         job.session()->source().types.begin();
         i != job.session()->source().types.end();
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
      return SAVE;
  }

  if (wait_interval_.get())
    return DecideWhileInWaitInterval(job);

  if (mode_ == CONFIGURATION_MODE) {
    if (job.purpose() == SyncSessionJob::NUDGE)
      return SAVE;  // Running requires a mode switch.
    else if (job.purpose() == SyncSessionJob::CONFIGURATION)
      return CONTINUE;
    else
      return DROP;
  }

  // We are in normal mode.
  DCHECK_EQ(mode_, NORMAL_MODE);
  DCHECK_NE(job.purpose(), SyncSessionJob::CONFIGURATION);

  // Note about some subtle scheduling semantics.
  //
  // It's possible at this point that |job| is known to be unnecessary, and
  // dropping it would be perfectly safe and correct. Consider
  //
  // 1) |job| is a POLL with a |scheduled_start| time that is less than
  //    the time that the last successful all-datatype NUDGE completed.
  //
  // 2) |job| is a NUDGE (for any combination of types) with a
  //    |scheduled_start| time that is less than the time that the last
  //    successful all-datatype NUDGE completed, and it has a NOTIFICATION
  //    GetUpdatesCallerInfo value yet offers no new notification hint.
  //
  // 3) |job| is a NUDGE with a |scheduled_start| time that is less than
  //    the time that the last successful matching-datatype NUDGE completed,
  //    and payloads (hints) are identical to that last successful NUDGE.
  //
  //  Case 1 can occur if the POLL timer fires *after* a call to
  //  ScheduleSyncSessionJob for a NUDGE, but *before* the thread actually
  //  picks the resulting posted task off of the MessageLoop. The NUDGE will
  //  run first and complete at a time greater than the POLL scheduled_start.
  //  However, this case (and POLLs in general) is so rare that we ignore it (
  //  and avoid the required bookeeping to simplify code).
  //
  //  We avoid cases 2 and 3 by externally synchronizing NUDGE requests --
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

void SyncSchedulerImpl::HandleSaveJobDecision(scoped_ptr<SyncSessionJob> job) {
  DCHECK_EQ(DecideOnJob(*job), SAVE);
  const bool is_nudge = job->purpose() == SyncSessionJob::NUDGE;
  if (is_nudge && pending_nudge_) {
    SDVLOG(2) << "Coalescing a pending nudge";
    // TODO(tim): This basically means we never use the more-careful coalescing
    // logic in ScheduleNudgeImpl that takes the min of the two nudge start
    // times, because we're calling this function first.  Pull this out
    // into a function to coalesce + set start times and reuse.
    pending_nudge_->mutable_session()->CoalesceSources(
        job->session()->source());
    return;
  }

  scoped_ptr<SyncSessionJob> job_to_save = job->CloneAndAbandon();
  if (wait_interval_.get() && !wait_interval_->pending_configure_job) {
    // This job should be made the new canary.
    if (is_nudge) {
      set_pending_nudge(job_to_save.get());
    } else {
      SDVLOG(2) << "Saving a configuration job";
      DCHECK_EQ(job->purpose(), SyncSessionJob::CONFIGURATION);
      DCHECK(!wait_interval_->pending_configure_job);
      DCHECK_EQ(mode_, CONFIGURATION_MODE);
      DCHECK(!job->config_params().ready_task.is_null());
      // The only nudge that could exist is a scheduled canary nudge.
      DCHECK(!unscheduled_nudge_storage_.get());
      if (pending_nudge_) {
        // Pre-empt the nudge canary and abandon the old nudge (owned by task).
        unscheduled_nudge_storage_ = pending_nudge_->CloneAndAbandon();
        set_pending_nudge(unscheduled_nudge_storage_.get());
      }
      wait_interval_->pending_configure_job = job_to_save.get();
    }
    TimeDelta length =
        wait_interval_->timer.desired_run_time() - TimeTicks::Now();
    wait_interval_->length = length < TimeDelta::FromSeconds(0) ?
        TimeDelta::FromSeconds(0) : length;
    RestartWaiting(job_to_save.Pass());
    return;
  }

  // Note that today there are no cases where we SAVE a CONFIGURATION job
  // when we're not in a WaitInterval. See bug 147736.
  DCHECK(is_nudge);
  // There may or may not be a pending_configure_job. Either way this nudge
  // is unschedulable.
  set_pending_nudge(job_to_save.get());
  unscheduled_nudge_storage_ = job_to_save.Pass();
}

// Functor for std::find_if to search by ModelSafeGroup.
struct ModelSafeWorkerGroupIs {
  explicit ModelSafeWorkerGroupIs(ModelSafeGroup group) : group(group) {}
  bool operator()(ModelSafeWorker* w) {
    return group == w->GetModelSafeGroup();
  }
  ModelSafeGroup group;
};

void SyncSchedulerImpl::ScheduleNudgeAsync(
    const TimeDelta& desired_delay,
    NudgeSource source, ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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

void SyncSchedulerImpl::ScheduleNudgeImpl(
    const TimeDelta& delay,
    GetUpdatesCallerInfo::GetUpdatesSource source,
    const ModelTypeInvalidationMap& invalidation_map,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(!invalidation_map.empty()) << "Nudge scheduled for no types!";

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
      CreateSyncSession(info).Pass(),
      ConfigurationParams(),
      nudge_location));
  job->set_destruction_observer(weak_ptr_factory_.GetWeakPtr());
  JobProcessDecision decision = DecideOnJob(*job);
  SDVLOG(2) << "Should run "
            << SyncSessionJob::GetPurposeString(job->purpose())
            << " job " << job->session()
            << " in mode " << GetModeString(mode_)
            << ": " << GetDecisionString(decision);
  if (decision != CONTINUE) {
    // End of the line, though we may save the job for later.
    if (decision == SAVE) {
      HandleSaveJobDecision(job.Pass());
    } else {
      DCHECK_EQ(decision, DROP);
    }
    return;
  }

  if (pending_nudge_) {
    SDVLOG(2) << "Rescheduling pending nudge";
    pending_nudge_->mutable_session()->CoalesceSources(
        job->session()->source());
    // Choose the start time as the earliest of the 2. Note that this means
    // if a nudge arrives with delay (e.g. kDefaultSessionsCommitDelaySeconds)
    // but a nudge is already scheduled to go out, we'll send the (tab) commit
    // without waiting.
    pending_nudge_->set_scheduled_start(
        std::min(job->scheduled_start(), pending_nudge_->scheduled_start()));
    // Abandon the old task by cloning and replacing the session.
    // It's possible that by "rescheduling" we're actually taking a job that
    // was previously unscheduled and giving it wings, so take care to reset
    // unscheduled nudge storage.
    job = pending_nudge_->CloneAndAbandon();
    pending_nudge_ = NULL;
    unscheduled_nudge_storage_.reset();
    // It's also possible we took a canary job, since we allow one nudge
    // per backoff interval.
    DCHECK(!wait_interval_ || !wait_interval_->had_nudge);
  }

  // TODO(zea): Consider adding separate throttling/backoff for datatype
  // refresh requests.
  ScheduleSyncSessionJob(job.Pass());
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

void SyncSchedulerImpl::PostTask(
    const tracked_objects::Location& from_here,
    const char* name, const base::Closure& task) {
  SDVLOG_LOC(from_here, 3) << "Posting " << name << " task";
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (!started_) {
    SDVLOG(1) << "Not posting task as scheduler is stopped.";
    return;
  }
  sync_loop_->PostTask(from_here, task);
}

void SyncSchedulerImpl::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const char* name, const base::Closure& task, base::TimeDelta delay) {
  SDVLOG_LOC(from_here, 3) << "Posting " << name << " task with "
                           << delay.InMilliseconds() << " ms delay";
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (!started_) {
    SDVLOG(1) << "Not posting task as scheduler is stopped.";
    return;
  }
  sync_loop_->PostDelayedTask(from_here, task, delay);
}

void SyncSchedulerImpl::ScheduleSyncSessionJob(
    scoped_ptr<SyncSessionJob> job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (no_scheduling_allowed_) {
    NOTREACHED() << "Illegal to schedule job while session in progress.";
    return;
  }

  TimeDelta delay = job->scheduled_start() - TimeTicks::Now();
  tracked_objects::Location loc(job->from_location());
  if (delay < TimeDelta::FromMilliseconds(0))
    delay = TimeDelta::FromMilliseconds(0);
  SDVLOG_LOC(loc, 2)
      << "In ScheduleSyncSessionJob with "
      << SyncSessionJob::GetPurposeString(job->purpose())
      << " job and " << delay.InMilliseconds() << " ms delay";

  DCHECK(job->purpose() == SyncSessionJob::NUDGE ||
         job->purpose() == SyncSessionJob::POLL);
  if (job->purpose() == SyncSessionJob::NUDGE) {
    SDVLOG_LOC(loc, 2) << "Resetting pending_nudge to ";
    DCHECK(!pending_nudge_ || pending_nudge_->session() ==
           job->session());
    set_pending_nudge(job.get());
  }

  PostDelayedTask(loc, "DoSyncSessionJob",
      base::Bind(base::IgnoreResult(&SyncSchedulerImpl::DoSyncSessionJob),
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::Passed(&job)),
      delay);
}

bool SyncSchedulerImpl::DoSyncSessionJob(scoped_ptr<SyncSessionJob> job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (job->purpose() == SyncSessionJob::NUDGE) {
    if (pending_nudge_ == NULL ||
        pending_nudge_->session() != job->session()) {
      // |job| is abandoned.
      SDVLOG(2) << "Dropping a nudge in "
                << "DoSyncSessionJob because another nudge was scheduled";
      return false;
    }
    pending_nudge_ = NULL;
  }

  base::AutoReset<bool> protector(&no_scheduling_allowed_, true);
  JobProcessDecision decision = DecideOnJob(*job);
  SDVLOG(2) << "Should run "
            << SyncSessionJob::GetPurposeString(job->purpose())
            << " job " << job->session()
            << " in mode " << GetModeString(mode_)
            << " with source " << job->session()->source().updates_source
            << ": " << GetDecisionString(decision);
  if (decision != CONTINUE) {
    if (decision == SAVE) {
      HandleSaveJobDecision(job.Pass());
    } else {
      DCHECK_EQ(decision, DROP);
    }
    return false;
  }

  SDVLOG(2) << "Calling SyncShare with "
            << SyncSessionJob::GetPurposeString(job->purpose()) << " job";
  bool premature_exit = !syncer_->SyncShare(job->mutable_session(),
                                            job->start_step(),
                                            job->end_step());
  SDVLOG(2) << "Done SyncShare, returned: " << premature_exit;

  return FinishSyncSessionJob(job.Pass(), premature_exit);
}

void SyncSchedulerImpl::UpdateNudgeTimeRecords(const SyncSourceInfo& info) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

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

bool SyncSchedulerImpl::FinishSyncSessionJob(scoped_ptr<SyncSessionJob> job,
                                             bool exited_prematurely) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // Now update the status of the connection from SCM. We need this to decide
  // whether we need to save/run future jobs. The notifications from SCM are
  // not reliable.
  //
  // TODO(rlarocque): crbug.com/110954
  // We should get rid of the notifications and it is probably not needed to
  // maintain this status variable in 2 places. We should query it directly
  // from SCM when needed.
  ServerConnectionManager* scm = session_context_->connection_manager();
  UpdateServerConnectionManagerStatus(scm->server_status());

  // Let job know that we're through syncing (calling SyncShare) at this point.
  bool succeeded = false;
  {
    base::AutoReset<bool> protector(&no_scheduling_allowed_, true);
    succeeded = job->Finish(exited_prematurely);
  }

  SDVLOG(2) << "Updating the next polling time after SyncMain";
  ScheduleNextSync(job.Pass(), succeeded);
  return succeeded;
}

void SyncSchedulerImpl::set_pending_nudge(SyncSessionJob* job) {
  job->set_destruction_observer(weak_ptr_factory_.GetWeakPtr());
  pending_nudge_ = job;
}

void SyncSchedulerImpl::ScheduleNextSync(
    scoped_ptr<SyncSessionJob> finished_job, bool succeeded) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  AdjustPolling(finished_job.get());

  if (succeeded) {
    // No job currently supported by the scheduler could succeed without
    // successfully reaching the server.  Therefore, if we make it here, it is
    // appropriate to reset the backoff interval.
    wait_interval_.reset();
    NotifyRetryTime(base::Time());
    SDVLOG(2) << "Job succeeded so not scheduling more jobs";
    return;
  }

  if (IsSyncingCurrentlySilenced()) {
    SDVLOG(2) << "We are currently throttled; scheduling Unthrottle.";
    // If we're here, it's because |job| was silenced until a server specified
    // time. (Note, it had to be |job|, because DecideOnJob would not permit
    // any job through while in WaitInterval::THROTTLED).
    scoped_ptr<SyncSessionJob> clone = finished_job->Clone();
    if (clone->purpose() == SyncSessionJob::NUDGE)
      set_pending_nudge(clone.get());
    else if (clone->purpose() == SyncSessionJob::CONFIGURATION)
      wait_interval_->pending_configure_job = clone.get();
    else
      clone.reset();  // Unthrottling is enough, no need to force a canary.

    RestartWaiting(clone.Pass());
    return;
  }

  if (finished_job->purpose() == SyncSessionJob::POLL) {
    return; // We don't retry POLL jobs.
  }

  // TODO(rlarocque): There's no reason why we should blindly backoff and retry
  // if we don't succeed.  Some types of errors are not likely to disappear on
  // their own.  With the return values now available in the old_job.session,
  // we should be able to detect such errors and only retry when we detect
  // transient errors.

  if (IsBackingOff() && wait_interval_->timer.IsRunning() &&
      mode_ == NORMAL_MODE) {
    // When in normal mode, we allow up to one nudge per backoff interval.  It
    // appears that this was our nudge for this interval, and it failed.
    //
    // Note: This does not prevent us from running canary jobs.  For example,
    // an IP address change might still result in another nudge being executed
    // during this backoff interval.
    SDVLOG(2) << "A nudge during backoff failed, creating new pending nudge.";
    DCHECK_EQ(SyncSessionJob::NUDGE, finished_job->purpose());
    DCHECK(!wait_interval_->had_nudge);

    wait_interval_->had_nudge = true;
    DCHECK(!pending_nudge_);

    scoped_ptr<SyncSessionJob> new_job = finished_job->Clone();
    set_pending_nudge(new_job.get());
    RestartWaiting(new_job.Pass());
  } else {
    // Either this is the first failure or a consecutive failure after our
    // backoff timer expired.  We handle it the same way in either case.
    SDVLOG(2) << "Non-'backoff nudge' SyncShare job failed";
    HandleContinuationError(finished_job.Pass());
  }
}

void SyncSchedulerImpl::AdjustPolling(const SyncSessionJob* old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

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

void SyncSchedulerImpl::RestartWaiting(scoped_ptr<SyncSessionJob> job) {
  CHECK(wait_interval_.get());
  wait_interval_->timer.Stop();
  DCHECK(wait_interval_->length >= TimeDelta::FromSeconds(0));
  if (wait_interval_->mode == WaitInterval::THROTTLED) {
    wait_interval_->timer.Start(FROM_HERE, wait_interval_->length,
                                base::Bind(&SyncSchedulerImpl::Unthrottle,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::Passed(&job)));
  } else {
    wait_interval_->timer.Start(FROM_HERE, wait_interval_->length,
                                base::Bind(&SyncSchedulerImpl::DoCanaryJob,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::Passed(&job)));
  }
}

void SyncSchedulerImpl::HandleContinuationError(
    scoped_ptr<SyncSessionJob> old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (DCHECK_IS_ON()) {
    if (IsBackingOff()) {
      DCHECK(wait_interval_->timer.IsRunning() || old_job->is_canary());
    }
  }

  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length :
          delay_provider_->GetInitialDelay(
              old_job->session()->status_controller().model_neutral_state()));

  SDVLOG(2) << "In handle continuation error with "
            << SyncSessionJob::GetPurposeString(old_job->purpose())
            << " job. The time delta(ms) is "
            << length.InMilliseconds();

  // This will reset the had_nudge variable as well.
  wait_interval_.reset(new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF,
                                        length));
  NotifyRetryTime(base::Time::Now() + length);
  scoped_ptr<SyncSessionJob> new_job(old_job->CloneFromLocation(FROM_HERE));
  new_job->set_scheduled_start(TimeTicks::Now() + length);
  if (old_job->purpose() == SyncSessionJob::CONFIGURATION) {
    SDVLOG(2) << "Configuration did not succeed, scheduling retry.";
    // Config params should always get set.
    DCHECK(!old_job->config_params().ready_task.is_null());
    wait_interval_->pending_configure_job = new_job.get();
  } else {
    // We are not in configuration mode. So wait_interval's pending job
    // should be null.
    DCHECK(wait_interval_->pending_configure_job == NULL);
    DCHECK(!pending_nudge_);
    set_pending_nudge(new_job.get());
  }

  RestartWaiting(new_job.Pass());
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
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "StopImpl called";

  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  wait_interval_.reset();
  NotifyRetryTime(base::Time());
  poll_timer_.Stop();
  pending_nudge_ = NULL;
  unscheduled_nudge_storage_.reset();
  if (started_) {
    started_ = false;
  }
  if (!callback.is_null())
    callback.Run();
}

void SyncSchedulerImpl::DoCanaryJob(scoped_ptr<SyncSessionJob> to_be_canary) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "Do canary job";

  // Only set canary privileges here, when we are about to run the job. This
  // avoids confusion in managing canary bits during scheduling, when you
  // consider that mode switches (e.g., to config) can "pre-empt" a NUDGE that
  // was scheduled as canary, and send it to an "unscheduled" state.
  to_be_canary->GrantCanaryPrivilege();

  if (to_be_canary->purpose() == SyncSessionJob::NUDGE) {
    // TODO(tim): Bug 158313.  Remove this check.
    if (pending_nudge_ == NULL ||
        pending_nudge_->session() != to_be_canary->session()) {
      // |job| is abandoned.
      SDVLOG(2) << "Dropping a nudge in "
                << "DoSyncSessionJob because another nudge was scheduled";
      return;
    }
    DCHECK_EQ(pending_nudge_->session(), to_be_canary->session());
  }
  DoSyncSessionJob(to_be_canary.Pass());
}

scoped_ptr<SyncSessionJob> SyncSchedulerImpl::TakePendingJobForCurrentMode() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // If we find a scheduled pending_ job, abandon the old one and return a
  // a clone.  If unscheduled, just hand over ownership.
  scoped_ptr<SyncSessionJob> candidate;
  if (mode_ == CONFIGURATION_MODE && wait_interval_.get()
      && wait_interval_->pending_configure_job) {
    SDVLOG(2) << "Found pending configure job";
    candidate =
        wait_interval_->pending_configure_job->CloneAndAbandon().Pass();
    wait_interval_->pending_configure_job = candidate.get();
  } else if (mode_ == NORMAL_MODE && pending_nudge_) {
    SDVLOG(2) << "Found pending nudge job";
    candidate = pending_nudge_->CloneAndAbandon();
    set_pending_nudge(candidate.get());
    unscheduled_nudge_storage_.reset();
  }
  // If we took a job and there's a wait interval, we took the pending canary.
  if (candidate && wait_interval_)
    wait_interval_->timer.Stop();
  return candidate.Pass();
}

scoped_ptr<SyncSession> SyncSchedulerImpl::CreateSyncSession(
    const SyncSourceInfo& source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DVLOG(2) << "Creating sync session with routes "
           << ModelSafeRoutingInfoToString(session_context_->routing_info());

  SyncSourceInfo info(source);
  return scoped_ptr<SyncSession>(new SyncSession(session_context_, this, info));
}

void SyncSchedulerImpl::PollTimerCallback() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ModelSafeRoutingInfo r;
  ModelTypeInvalidationMap invalidation_map =
      ModelSafeRoutingInfoToInvalidationMap(r, std::string());
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, invalidation_map);
  scoped_ptr<SyncSession> s(CreateSyncSession(info));
  scoped_ptr<SyncSessionJob> job(new SyncSessionJob(SyncSessionJob::POLL,
                                                    TimeTicks::Now(),
                                                    s.Pass(),
                                                    ConfigurationParams(),
                                                    FROM_HERE));
  job->set_destruction_observer(weak_ptr_factory_.GetWeakPtr());
  ScheduleSyncSessionJob(job.Pass());
}

void SyncSchedulerImpl::Unthrottle(scoped_ptr<SyncSessionJob> to_be_canary) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);
  DCHECK(!to_be_canary.get() || pending_nudge_ == to_be_canary.get() ||
         wait_interval_->pending_configure_job == to_be_canary.get());
  SDVLOG(2) << "Unthrottled " << (to_be_canary.get() ? "with " : "without ")
           << "canary.";

  // We're no longer throttled, so clear the wait interval.
  wait_interval_.reset();
  NotifyRetryTime(base::Time());

  // We treat this as a 'canary' in the sense that it was originally scheduled
  // to run some time ago, failed, and we now want to retry, versus a job that
  // was just created (e.g via ScheduleNudgeImpl). The main implication is
  // that we're careful to update routing info (etc) with such potentially
  // stale canary jobs.
  if (to_be_canary.get()) {
    DoCanaryJob(to_be_canary.Pass());
  } else {
    DCHECK(!unscheduled_nudge_storage_.get());
  }
}

void SyncSchedulerImpl::Notify(SyncEngineEvent::EventCause cause) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->NotifyListeners(SyncEngineEvent(cause));
}

void SyncSchedulerImpl::NotifyRetryTime(base::Time retry_time) {
  SyncEngineEvent event(SyncEngineEvent::RETRY_TIME_CHANGED);
  event.retry_time = retry_time;
  session_context_->NotifyListeners(event);
}

bool SyncSchedulerImpl::IsBackingOff() const {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::EXPONENTIAL_BACKOFF;
}

void SyncSchedulerImpl::OnSilencedUntil(
    const base::TimeTicks& silenced_until) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  wait_interval_.reset(new WaitInterval(WaitInterval::THROTTLED,
                                        silenced_until - TimeTicks::Now()));
  NotifyRetryTime(base::Time::Now() + wait_interval_->length);
}

bool SyncSchedulerImpl::IsSyncingCurrentlySilenced() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::THROTTLED;
}

void SyncSchedulerImpl::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  syncer_short_poll_interval_seconds_ = new_interval;
}

void SyncSchedulerImpl::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  syncer_long_poll_interval_seconds_ = new_interval;
}

void SyncSchedulerImpl::OnReceivedSessionsCommitDelay(
    const base::TimeDelta& new_delay) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sessions_commit_delay_ = new_delay;
}

void SyncSchedulerImpl::OnShouldStopSyncingPermanently() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "OnShouldStopSyncingPermanently";
  syncer_->RequestEarlyExit();  // Thread-safe.
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncSchedulerImpl::OnActionableError(
    const sessions::SyncSessionSnapshot& snap) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "OnActionableError";
  SyncEngineEvent event(SyncEngineEvent::ACTIONABLE_ERROR);
  event.snapshot = snap;
  session_context_->NotifyListeners(event);
}

void SyncSchedulerImpl::OnSyncProtocolError(
    const sessions::SyncSessionSnapshot& snapshot) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (ShouldRequestEarlyExit(
          snapshot.model_neutral_state().sync_protocol_error)) {
    SDVLOG(2) << "Sync Scheduler requesting early exit.";
    syncer_->RequestEarlyExit();  // Thread-safe.
  }
  if (IsActionableError(snapshot.model_neutral_state().sync_protocol_error))
    OnActionableError(snapshot);
}

void SyncSchedulerImpl::SetNotificationsEnabled(bool notifications_enabled) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->set_notifications_enabled(notifications_enabled);
}

base::TimeDelta SyncSchedulerImpl::GetSessionsCommitDelay() const {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return sessions_commit_delay_;
}

#undef SDVLOG_LOC

#undef SDVLOG

#undef SLOG

#undef ENUM_CASE

}  // namespace syncer
