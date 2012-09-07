// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_scheduler_impl.h"

#include <algorithm>
#include <cstring>

#include "base/auto_reset.h"
#include "base/bind.h"
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
      had_nudge(false) {
}

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

SyncSchedulerImpl::SyncSessionJob::SyncSessionJob()
    : purpose(UNKNOWN),
      is_canary_job(false) {
}

SyncSchedulerImpl::SyncSessionJob::~SyncSessionJob() {}

SyncSchedulerImpl::SyncSessionJob::SyncSessionJob(SyncSessionJobPurpose purpose,
    base::TimeTicks start,
    linked_ptr<sessions::SyncSession> session,
    bool is_canary_job,
    const ConfigurationParams& config_params,
    const tracked_objects::Location& from_here)
    : purpose(purpose),
      scheduled_start(start),
      session(session),
      is_canary_job(is_canary_job),
      config_params(config_params),
      from_here(from_here) {
}

const char* SyncSchedulerImpl::SyncSessionJob::GetPurposeString(
    SyncSchedulerImpl::SyncSessionJob::SyncSessionJobPurpose purpose) {
  switch (purpose) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(POLL);
    ENUM_CASE(NUDGE);
    ENUM_CASE(CONFIGURATION);
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
    case NUDGE_SOURCE_CONTINUATION:
      return GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION;
    case NUDGE_SOURCE_LOCAL_REFRESH:
      return GetUpdatesCallerInfo::DATATYPE_REFRESH;
    case NUDGE_SOURCE_UNKNOWN:
      return GetUpdatesCallerInfo::UNKNOWN;
    default:
      NOTREACHED();
      return GetUpdatesCallerInfo::UNKNOWN;
  }
}

SyncSchedulerImpl::WaitInterval::WaitInterval(Mode mode, TimeDelta length)
    : mode(mode), had_nudge(false), length(length) { }

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

void SyncSchedulerImpl::OnCredentialsUpdated() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  // TODO(lipalani): crbug.com/106262. One issue here is that if after
  // the auth error we happened to do gettime and it succeeded then
  // the |connection_code_| would be briefly OK however it would revert
  // back to SYNC_AUTH_ERROR at the end of the sync cycle. The
  // referenced bug explores the option of removing gettime calls
  // altogethere
  if (HttpResponse::SYNC_AUTH_ERROR == connection_code_) {
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
  PostTask(FROM_HERE, "DoCanaryJob",
           base::Bind(&SyncSchedulerImpl::DoCanaryJob,
                      weak_ptr_factory_.GetWeakPtr()));

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
      DCHECK(!wait_interval_.get() ||
             !wait_interval_->pending_configure_job.get());
    }

    DoPendingJobIfPossible(false);
  }
}

void SyncSchedulerImpl::SendInitialSnapshot() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  scoped_ptr<SyncSession> dummy(new SyncSession(session_context_, this,
      SyncSourceInfo(), ModelSafeRoutingInfo(),
      std::vector<ModelSafeWorker*>()));
  SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
  event.snapshot = dummy->TakeSnapshot();
  session_context_->NotifyListeners(event);
}

namespace {

// Helper to extract the routing info and workers corresponding to types in
// |types| from |current_routes| and |current_workers|.
void BuildModelSafeParams(
    ModelTypeSet types_to_download,
    const ModelSafeRoutingInfo& current_routes,
    const std::vector<ModelSafeWorker*>& current_workers,
    ModelSafeRoutingInfo* result_routes,
    std::vector<ModelSafeWorker*>* result_workers) {
  std::set<ModelSafeGroup> active_groups;
  active_groups.insert(GROUP_PASSIVE);
  for (ModelTypeSet::Iterator iter = types_to_download.First(); iter.Good();
       iter.Inc()) {
    ModelType type = iter.Get();
    ModelSafeRoutingInfo::const_iterator route = current_routes.find(type);
    DCHECK(route != current_routes.end());
    ModelSafeGroup group = route->second;
    (*result_routes)[type] = group;
    active_groups.insert(group);
  }

  for(std::vector<ModelSafeWorker*>::const_iterator iter =
          current_workers.begin(); iter != current_workers.end(); ++iter) {
    if (active_groups.count((*iter)->GetModelSafeGroup()) > 0)
      result_workers->push_back(*iter);
  }
}

}  // namespace.

bool SyncSchedulerImpl::ScheduleConfiguration(
    const ConfigurationParams& params) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(IsConfigRelatedUpdateSourceValue(params.source));
  DCHECK_EQ(CONFIGURATION_MODE, mode_);
  DCHECK(!params.ready_task.is_null());
  SDVLOG(2) << "Reconfiguring syncer.";

  // Only one configuration is allowed at a time. Verify we're not waiting
  // for a pending configure job.
  DCHECK(!wait_interval_.get() || !wait_interval_->pending_configure_job.get());

  // TODO(sync): now that ModelChanging commands only use those workers within
  // the routing info, we don't really need |restricted_workers|. Remove it.
  // crbug.com/133030
  ModelSafeRoutingInfo restricted_routes;
  std::vector<ModelSafeWorker*> restricted_workers;
  BuildModelSafeParams(params.types_to_download,
                       params.routing_info,
                       session_context_->workers(),
                       &restricted_routes,
                       &restricted_workers);
  session_context_->set_routing_info(params.routing_info);

  // Only reconfigure if we have types to download.
  if (!params.types_to_download.Empty()) {
    DCHECK(!restricted_routes.empty());
    linked_ptr<SyncSession> session(new SyncSession(
        session_context_,
        this,
        SyncSourceInfo(params.source,
                       ModelSafeRoutingInfoToStateMap(
                           restricted_routes,
                           std::string())),
        restricted_routes,
        restricted_workers));
    SyncSessionJob job(SyncSessionJob::CONFIGURATION,
                       TimeTicks::Now(),
                       session,
                       false,
                       params,
                       FROM_HERE);
    DoSyncSessionJob(job);

    // If we failed, the job would have been saved as the pending configure
    // job and a wait interval would have been set.
    if (!session->Succeeded()) {
      DCHECK(wait_interval_.get() &&
             wait_interval_->pending_configure_job.get());
      return false;
    }
  } else {
    SDVLOG(2) << "No change in routing info, calling ready task directly.";
    params.ready_task.Run();
  }

  return true;
}

SyncSchedulerImpl::JobProcessDecision
SyncSchedulerImpl::DecideWhileInWaitInterval(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(wait_interval_.get());

  SDVLOG(2) << "DecideWhileInWaitInterval with WaitInterval mode "
            << WaitInterval::GetModeString(wait_interval_->mode)
            << (wait_interval_->had_nudge ? " (had nudge)" : "")
            << (job.is_canary_job ? " (canary)" : "");

  if (job.purpose == SyncSessionJob::POLL)
    return DROP;

  DCHECK(job.purpose == SyncSessionJob::NUDGE ||
         job.purpose == SyncSessionJob::CONFIGURATION);
  if (wait_interval_->mode == WaitInterval::THROTTLED)
    return SAVE;

  DCHECK_EQ(wait_interval_->mode, WaitInterval::EXPONENTIAL_BACKOFF);
  if (job.purpose == SyncSessionJob::NUDGE) {
    if (mode_ == CONFIGURATION_MODE)
      return SAVE;

    // If we already had one nudge then just drop this nudge. We will retry
    // later when the timer runs out.
    if (!job.is_canary_job)
      return wait_interval_->had_nudge ? DROP : CONTINUE;
    else // We are here because timer ran out. So retry.
      return CONTINUE;
  }
  return job.is_canary_job ? CONTINUE : SAVE;
}

SyncSchedulerImpl::JobProcessDecision SyncSchedulerImpl::DecideOnJob(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  // See if our type is throttled.
  ModelTypeSet throttled_types =
      session_context_->throttled_data_type_tracker()->GetThrottledTypes();
  if (job.purpose == SyncSessionJob::NUDGE &&
      job.session->source().updates_source == GetUpdatesCallerInfo::LOCAL) {
    ModelTypeSet requested_types;
    for (ModelTypeStateMap::const_iterator i =
         job.session->source().types.begin();
         i != job.session->source().types.end();
         ++i) {
      requested_types.Put(i->first);
    }

    if (!requested_types.Empty() && throttled_types.HasAll(requested_types))
      return SAVE;
  }

  if (wait_interval_.get())
    return DecideWhileInWaitInterval(job);

  if (mode_ == CONFIGURATION_MODE) {
    if (job.purpose == SyncSessionJob::NUDGE)
      return SAVE;
    else if (job.purpose == SyncSessionJob::CONFIGURATION)
      return CONTINUE;
    else
      return DROP;
  }

  // We are in normal mode.
  DCHECK_EQ(mode_, NORMAL_MODE);
  DCHECK_NE(job.purpose, SyncSessionJob::CONFIGURATION);

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
  return job.purpose == SyncSessionJob::NUDGE ? SAVE : DROP;
}

void SyncSchedulerImpl::InitOrCoalescePendingJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(job.purpose != SyncSessionJob::CONFIGURATION);
  if (pending_nudge_.get() == NULL) {
    SDVLOG(2) << "Creating a pending nudge job";
    SyncSession* s = job.session.get();

    // Get a fresh session with similar configuration as before (resets
    // StatusController).
    scoped_ptr<SyncSession> session(new SyncSession(s->context(),
        s->delegate(), s->source(), s->routing_info(), s->workers()));

    SyncSessionJob new_job(SyncSessionJob::NUDGE, job.scheduled_start,
        make_linked_ptr(session.release()), false,
        ConfigurationParams(), job.from_here);
    pending_nudge_.reset(new SyncSessionJob(new_job));
    return;
  }

  SDVLOG(2) << "Coalescing a pending nudge";
  pending_nudge_->session->Coalesce(*(job.session.get()));
  pending_nudge_->scheduled_start = job.scheduled_start;

  // Unfortunately the nudge location cannot be modified. So it stores the
  // location of the first caller.
}

bool SyncSchedulerImpl::ShouldRunJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(started_);

  JobProcessDecision decision = DecideOnJob(job);
  SDVLOG(2) << "Should run "
            << SyncSessionJob::GetPurposeString(job.purpose)
            << " job in mode " << GetModeString(mode_)
            << ": " << GetDecisionString(decision);
  if (decision != SAVE)
    return decision == CONTINUE;

  DCHECK(job.purpose == SyncSessionJob::NUDGE || job.purpose ==
      SyncSessionJob::CONFIGURATION);

  SaveJob(job);
  return false;
}

void SyncSchedulerImpl::SaveJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (job.purpose == SyncSessionJob::NUDGE) {
    SDVLOG(2) << "Saving a nudge job";
    InitOrCoalescePendingJob(job);
  } else if (job.purpose == SyncSessionJob::CONFIGURATION){
    SDVLOG(2) << "Saving a configuration job";
    DCHECK(wait_interval_.get());
    DCHECK(mode_ == CONFIGURATION_MODE);

    // Config params should always get set.
    DCHECK(!job.config_params.ready_task.is_null());
    SyncSession* old = job.session.get();
    SyncSession* s(new SyncSession(session_context_, this, old->source(),
                                   old->routing_info(), old->workers()));
    SyncSessionJob new_job(job.purpose,
                           TimeTicks::Now(),
                           make_linked_ptr(s),
                           false,
                           job.config_params,
                           job.from_here);
    wait_interval_->pending_configure_job.reset(new SyncSessionJob(new_job));
  } // drop the rest.
  // TODO(sync): Is it okay to drop the rest?  It's weird that
  // SaveJob() only does what it says sometimes.  (See
  // http://crbug.com/90868.)
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
    const TimeDelta& delay,
    NudgeSource source, ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay " << delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "types " << ModelTypeSetToString(types);

  ModelTypeStateMap type_state_map =
      ModelTypeSetToStateMap(types, std::string());
  SyncSchedulerImpl::ScheduleNudgeImpl(delay,
                                       GetUpdatesFromNudgeSource(source),
                                       type_state_map,
                                       false,
                                       nudge_location);
}

void SyncSchedulerImpl::ScheduleNudgeWithStatesAsync(
    const TimeDelta& delay,
    NudgeSource source, const ModelTypeStateMap& type_state_map,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay " << delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "payloads "
      << ModelTypeStateMapToString(type_state_map);

  SyncSchedulerImpl::ScheduleNudgeImpl(delay,
                                       GetUpdatesFromNudgeSource(source),
                                       type_state_map,
                                       false,
                                       nudge_location);
}

void SyncSchedulerImpl::ScheduleNudgeImpl(
    const TimeDelta& delay,
    GetUpdatesCallerInfo::GetUpdatesSource source,
    const ModelTypeStateMap& type_state_map,
    bool is_canary_job, const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(!type_state_map.empty()) << "Nudge scheduled for no types!";

  SDVLOG_LOC(nudge_location, 2)
      << "In ScheduleNudgeImpl with delay "
      << delay.InMilliseconds() << " ms, "
      << "source " << GetUpdatesSourceString(source) << ", "
      << "payloads "
      << ModelTypeStateMapToString(type_state_map)
      << (is_canary_job ? " (canary)" : "");

  SyncSourceInfo info(source, type_state_map);
  UpdateNudgeTimeRecords(info);

  SyncSession* session(CreateSyncSession(info));
  SyncSessionJob job(SyncSessionJob::NUDGE, TimeTicks::Now() + delay,
                     make_linked_ptr(session), is_canary_job,
                     ConfigurationParams(), nudge_location);

  session = NULL;
  if (!ShouldRunJob(job))
    return;

  if (pending_nudge_.get()) {
    if (IsBackingOff() && delay > TimeDelta::FromSeconds(1)) {
      SDVLOG(2) << "Dropping the nudge because we are in backoff";
      return;
    }

    SDVLOG(2) << "Coalescing pending nudge";
    pending_nudge_->session->Coalesce(*(job.session.get()));

    SDVLOG(2) << "Rescheduling pending nudge";
    SyncSession* s = pending_nudge_->session.get();
    job.session.reset(new SyncSession(s->context(), s->delegate(),
        s->source(), s->routing_info(), s->workers()));

    // Choose the start time as the earliest of the 2.
    job.scheduled_start = std::min(job.scheduled_start,
                                   pending_nudge_->scheduled_start);
    pending_nudge_.reset();
  }

  // TODO(zea): Consider adding separate throttling/backoff for datatype
  // refresh requests.
  ScheduleSyncSessionJob(job);
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

// static
void SyncSchedulerImpl::SetSyncerStepsForPurpose(
    SyncSessionJob::SyncSessionJobPurpose purpose,
    SyncerStep* start,
    SyncerStep* end) {
  switch (purpose) {
    case SyncSessionJob::CONFIGURATION:
      *start = DOWNLOAD_UPDATES;
      *end = APPLY_UPDATES;
      return;
    case SyncSessionJob::NUDGE:
    case SyncSessionJob::POLL:
      *start = SYNCER_BEGIN;
      *end = SYNCER_END;
      return;
    default:
      NOTREACHED();
      *start = SYNCER_END;
      *end = SYNCER_END;
      return;
  }
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

void SyncSchedulerImpl::ScheduleSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (no_scheduling_allowed_) {
    NOTREACHED() << "Illegal to schedule job while session in progress.";
    return;
  }

  TimeDelta delay = job.scheduled_start - TimeTicks::Now();
  if (delay < TimeDelta::FromMilliseconds(0))
    delay = TimeDelta::FromMilliseconds(0);
  SDVLOG_LOC(job.from_here, 2)
      << "In ScheduleSyncSessionJob with "
      << SyncSessionJob::GetPurposeString(job.purpose)
      << " job and " << delay.InMilliseconds() << " ms delay";

  DCHECK(job.purpose == SyncSessionJob::NUDGE ||
         job.purpose == SyncSessionJob::POLL);
  if (job.purpose == SyncSessionJob::NUDGE) {
    SDVLOG_LOC(job.from_here, 2) << "Resetting pending_nudge";
    DCHECK(!pending_nudge_.get() || pending_nudge_->session.get() ==
           job.session);
    pending_nudge_.reset(new SyncSessionJob(job));
  }
  PostDelayedTask(job.from_here, "DoSyncSessionJob",
                  base::Bind(&SyncSchedulerImpl::DoSyncSessionJob,
                             weak_ptr_factory_.GetWeakPtr(),
                             job),
                  delay);
}

void SyncSchedulerImpl::DoSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  AutoReset<bool> protector(&no_scheduling_allowed_, true);
  if (!ShouldRunJob(job)) {
    SLOG(WARNING)
        << "Not executing "
        << SyncSessionJob::GetPurposeString(job.purpose) << " job from "
        << GetUpdatesSourceString(job.session->source().updates_source);
    return;
  }

  if (job.purpose == SyncSessionJob::NUDGE) {
    if (pending_nudge_.get() == NULL ||
        pending_nudge_->session != job.session) {
      SDVLOG(2) << "Dropping a nudge in "
                << "DoSyncSessionJob because another nudge was scheduled";
      return;  // Another nudge must have been scheduled in in the meantime.
    }
    pending_nudge_.reset();

    // Create the session with the latest model safe table and use it to purge
    // and update any disabled or modified entries in the job.
    scoped_ptr<SyncSession> session(CreateSyncSession(job.session->source()));

    job.session->RebaseRoutingInfoWithLatest(*session);
  }
  SDVLOG(2) << "DoSyncSessionJob with "
            << SyncSessionJob::GetPurposeString(job.purpose) << " job";

  SyncerStep begin(SYNCER_END);
  SyncerStep end(SYNCER_END);
  SetSyncerStepsForPurpose(job.purpose, &begin, &end);

  bool has_more_to_sync = true;
  while (ShouldRunJob(job) && has_more_to_sync) {
    SDVLOG(2) << "Calling SyncShare.";
    // Synchronously perform the sync session from this thread.
    syncer_->SyncShare(job.session.get(), begin, end);
    has_more_to_sync = job.session->HasMoreToSync();
    if (has_more_to_sync)
      job.session->PrepareForAnotherSyncCycle();
  }
  SDVLOG(2) << "Done SyncShare looping.";

  FinishSyncSessionJob(job);
}

void SyncSchedulerImpl::UpdateNudgeTimeRecords(const SyncSourceInfo& info) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  // We are interested in recording time between local nudges for datatypes.
  // TODO(tim): Consider tracking LOCAL_NOTIFICATION as well.
  if (info.updates_source != GetUpdatesCallerInfo::LOCAL)
    return;

  base::TimeTicks now = TimeTicks::Now();
  // Update timing information for how often datatypes are triggering nudges.
  for (ModelTypeStateMap::const_iterator iter = info.types.begin();
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

void SyncSchedulerImpl::FinishSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // Now update the status of the connection from SCM. We need this to decide
  // whether we need to save/run future jobs. The notifications from SCM are not
  // reliable.
  //
  // TODO(rlarocque): crbug.com/110954
  // We should get rid of the notifications and it is probably not needed to
  // maintain this status variable in 2 places. We should query it directly from
  // SCM when needed.
  ServerConnectionManager* scm = session_context_->connection_manager();
  UpdateServerConnectionManagerStatus(scm->server_status());

  if (IsSyncingCurrentlySilenced()) {
    SDVLOG(2) << "We are currently throttled; not scheduling the next sync.";
    // TODO(sync): Investigate whether we need to check job.purpose
    // here; see DCHECKs in SaveJob().  (See http://crbug.com/90868.)
    SaveJob(job);
    return;  // Nothing to do.
  } else if (job.session->Succeeded() &&
             !job.config_params.ready_task.is_null()) {
    // If this was a configuration job with a ready task, invoke it now that
    // we finished successfully.
    AutoReset<bool> protector(&no_scheduling_allowed_, true);
    job.config_params.ready_task.Run();
  }

  SDVLOG(2) << "Updating the next polling time after SyncMain";
  ScheduleNextSync(job);
}

void SyncSchedulerImpl::ScheduleNextSync(const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(!old_job.session->HasMoreToSync());

  AdjustPolling(&old_job);

  if (old_job.session->Succeeded()) {
    // Only reset backoff if we actually reached the server.
    if (old_job.session->SuccessfullyReachedServer())
      wait_interval_.reset();
    SDVLOG(2) << "Job succeeded so not scheduling more jobs";
    return;
  }

  if (old_job.purpose == SyncSessionJob::POLL) {
    return; // We don't retry POLL jobs.
  }

  // TODO(rlarocque): There's no reason why we should blindly backoff and retry
  // if we don't succeed.  Some types of errors are not likely to disappear on
  // their own.  With the return values now available in the old_job.session, we
  // should be able to detect such errors and only retry when we detect
  // transient errors.

  if (IsBackingOff() && wait_interval_->timer.IsRunning() &&
      mode_ == NORMAL_MODE) {
    // When in normal mode, we allow up to one nudge per backoff interval.  It
    // appears that this was our nudge for this interval, and it failed.
    //
    // Note: This does not prevent us from running canary jobs.  For example, an
    // IP address change might still result in another nudge being executed
    // during this backoff interval.
    SDVLOG(2) << "A nudge during backoff failed";

    DCHECK_EQ(SyncSessionJob::NUDGE, old_job.purpose);
    DCHECK(!wait_interval_->had_nudge);

    wait_interval_->had_nudge = true;
    InitOrCoalescePendingJob(old_job);
    RestartWaiting();
  } else {
    // Either this is the first failure or a consecutive failure after our
    // backoff timer expired.  We handle it the same way in either case.
    SDVLOG(2) << "Non-'backoff nudge' SyncShare job failed";
    HandleContinuationError(old_job);
  }
}

void SyncSchedulerImpl::AdjustPolling(const SyncSessionJob* old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  TimeDelta poll  = (!session_context_->notifications_enabled()) ?
      syncer_short_poll_interval_seconds_ :
      syncer_long_poll_interval_seconds_;
  bool rate_changed = !poll_timer_.IsRunning() ||
                       poll != poll_timer_.GetCurrentDelay();

  if (old_job && old_job->purpose != SyncSessionJob::POLL && !rate_changed)
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
  wait_interval_->timer.Stop();
  wait_interval_->timer.Start(FROM_HERE, wait_interval_->length,
                              this, &SyncSchedulerImpl::DoCanaryJob);
}

void SyncSchedulerImpl::HandleContinuationError(
    const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (DCHECK_IS_ON()) {
    if (IsBackingOff()) {
      DCHECK(wait_interval_->timer.IsRunning() || old_job.is_canary_job);
    }
  }

  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length :
          delay_provider_->GetInitialDelay(
              old_job.session->status_controller().model_neutral_state()));

  SDVLOG(2) << "In handle continuation error with "
            << SyncSessionJob::GetPurposeString(old_job.purpose)
            << " job. The time delta(ms) is "
            << length.InMilliseconds();

  // This will reset the had_nudge variable as well.
  wait_interval_.reset(new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF,
                                        length));
  if (old_job.purpose == SyncSessionJob::CONFIGURATION) {
    SDVLOG(2) << "Configuration did not succeed, scheduling retry.";
    // Config params should always get set.
    DCHECK(!old_job.config_params.ready_task.is_null());
    SyncSession* old = old_job.session.get();
    SyncSession* s(new SyncSession(session_context_, this,
        old->source(), old->routing_info(), old->workers()));
    SyncSessionJob job(old_job.purpose, TimeTicks::Now() + length,
                       make_linked_ptr(s), false, old_job.config_params,
                       FROM_HERE);
    wait_interval_->pending_configure_job.reset(new SyncSessionJob(job));
  } else {
    // We are not in configuration mode. So wait_interval's pending job
    // should be null.
    DCHECK(wait_interval_->pending_configure_job.get() == NULL);

    // TODO(lipalani) - handle clear user data.
    InitOrCoalescePendingJob(old_job);
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
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "StopImpl called";

  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  wait_interval_.reset();
  poll_timer_.Stop();
  if (started_) {
    started_ = false;
  }
  if (!callback.is_null())
    callback.Run();
}

void SyncSchedulerImpl::DoCanaryJob() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "Do canary job";
  DoPendingJobIfPossible(true);
}

void SyncSchedulerImpl::DoPendingJobIfPossible(bool is_canary_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SyncSessionJob* job_to_execute = NULL;
  if (mode_ == CONFIGURATION_MODE && wait_interval_.get()
      && wait_interval_->pending_configure_job.get()) {
    SDVLOG(2) << "Found pending configure job";
    job_to_execute = wait_interval_->pending_configure_job.get();
  } else if (mode_ == NORMAL_MODE && pending_nudge_.get()) {
    SDVLOG(2) << "Found pending nudge job";

    scoped_ptr<SyncSession> session(CreateSyncSession(
        pending_nudge_->session->source()));

    // Also the routing info might have been changed since we cached the
    // pending nudge. Update it by coalescing to the latest.
    pending_nudge_->session->Coalesce(*(session.get()));
    // The pending nudge would be cleared in the DoSyncSessionJob function.
    job_to_execute = pending_nudge_.get();
  }

  if (job_to_execute != NULL) {
    SDVLOG(2) << "Executing pending job";
    SyncSessionJob copy = *job_to_execute;
    copy.is_canary_job = is_canary_job;
    DoSyncSessionJob(copy);
  }
}

SyncSession* SyncSchedulerImpl::CreateSyncSession(
    const SyncSourceInfo& source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DVLOG(2) << "Creating sync session with routes "
           << ModelSafeRoutingInfoToString(session_context_->routing_info());

  SyncSourceInfo info(source);
  SyncSession* session(new SyncSession(session_context_, this, info,
      session_context_->routing_info(), session_context_->workers()));

  return session;
}

void SyncSchedulerImpl::PollTimerCallback() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ModelSafeRoutingInfo r;
  ModelTypeStateMap type_state_map =
      ModelSafeRoutingInfoToStateMap(r, std::string());
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, type_state_map);
  SyncSession* s = CreateSyncSession(info);

  SyncSessionJob job(SyncSessionJob::POLL, TimeTicks::Now(),
                     make_linked_ptr(s),
                     false,
                     ConfigurationParams(),
                     FROM_HERE);

  ScheduleSyncSessionJob(job);
}

void SyncSchedulerImpl::Unthrottle() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);
  SDVLOG(2) << "Unthrottled.";
  DoCanaryJob();
  wait_interval_.reset();
}

void SyncSchedulerImpl::Notify(SyncEngineEvent::EventCause cause) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->NotifyListeners(SyncEngineEvent(cause));
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
  wait_interval_->timer.Start(FROM_HERE, wait_interval_->length, this,
      &SyncSchedulerImpl::Unthrottle);
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
