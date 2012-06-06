// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_scheduler.h"

#include <algorithm>
#include <cstring>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "sync/engine/syncer.h"
#include "sync/protocol/proto_enum_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/data_type_histogram.h"
#include "sync/util/logging.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sessions::SyncSourceInfo;
using syncable::ModelTypeSet;
using syncable::ModelTypeSetToString;
using syncable::ModelTypePayloadMap;
using sync_pb::GetUpdatesCallerInfo;

namespace {
bool ShouldRequestEarlyExit(
    const browser_sync::SyncProtocolError& error) {
  switch (error.error_type) {
    case browser_sync::SYNC_SUCCESS:
    case browser_sync::MIGRATION_DONE:
    case browser_sync::THROTTLED:
    case browser_sync::TRANSIENT_ERROR:
      return false;
    case browser_sync::NOT_MY_BIRTHDAY:
    case browser_sync::CLEAR_PENDING:
      // If we send terminate sync early then |sync_cycle_ended| notification
      // would not be sent. If there were no actions then |ACTIONABLE_ERROR|
      // notification wouldnt be sent either. Then the UI layer would be left
      // waiting forever. So assert we would send something.
      DCHECK(error.action != browser_sync::UNKNOWN_ACTION);
      return true;
    case browser_sync::INVALID_CREDENTIAL:
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
    const browser_sync::SyncProtocolError& error) {
  return (error.action != browser_sync::UNKNOWN_ACTION);
}
}  // namespace

SyncScheduler::DelayProvider::DelayProvider() {}
SyncScheduler::DelayProvider::~DelayProvider() {}

SyncScheduler::WaitInterval::WaitInterval()
    : mode(UNKNOWN),
      had_nudge(false) {
}

SyncScheduler::WaitInterval::~WaitInterval() {}

#define ENUM_CASE(x) case x: return #x; break;

const char* SyncScheduler::WaitInterval::GetModeString(Mode mode) {
  switch (mode) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(EXPONENTIAL_BACKOFF);
    ENUM_CASE(THROTTLED);
  }
  NOTREACHED();
  return "";
}

SyncScheduler::SyncSessionJob::SyncSessionJob()
    : purpose(UNKNOWN),
      is_canary_job(false) {
}

SyncScheduler::SyncSessionJob::~SyncSessionJob() {}

SyncScheduler::SyncSessionJob::SyncSessionJob(SyncSessionJobPurpose purpose,
    base::TimeTicks start,
    linked_ptr<sessions::SyncSession> session, bool is_canary_job,
    const tracked_objects::Location& from_here) : purpose(purpose),
        scheduled_start(start),
        session(session),
        is_canary_job(is_canary_job),
        from_here(from_here) {
}

const char* SyncScheduler::SyncSessionJob::GetPurposeString(
    SyncScheduler::SyncSessionJob::SyncSessionJobPurpose purpose) {
  switch (purpose) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(POLL);
    ENUM_CASE(NUDGE);
    ENUM_CASE(CLEAR_USER_DATA);
    ENUM_CASE(CONFIGURATION);
    ENUM_CASE(CLEANUP_DISABLED_TYPES);
  }
  NOTREACHED();
  return "";
}

TimeDelta SyncScheduler::DelayProvider::GetDelay(
    const base::TimeDelta& last_delay) {
  return SyncScheduler::GetRecommendedDelay(last_delay);
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

SyncScheduler::WaitInterval::WaitInterval(Mode mode, TimeDelta length)
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

SyncScheduler::SyncScheduler(const std::string& name,
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
      delay_provider_(new DelayProvider()),
      syncer_(syncer),
      session_context_(context) {
  DCHECK(sync_loop_);
}

SyncScheduler::~SyncScheduler() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  StopImpl(base::Closure());
}

void SyncScheduler::OnCredentialsUpdated() {
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

void SyncScheduler::OnConnectionStatusChange() {
  if (HttpResponse::CONNECTION_UNAVAILABLE  == connection_code_) {
    // Optimistically assume that the connection is fixed and try
    // connecting.
    OnServerConnectionErrorFixed();
  }
}

void SyncScheduler::OnServerConnectionErrorFixed() {
  connection_code_ = HttpResponse::SERVER_CONNECTION_OK;
  PostTask(FROM_HERE, "DoCanaryJob",
           base::Bind(&SyncScheduler::DoCanaryJob,
                      weak_ptr_factory_.GetWeakPtr()));

}

void SyncScheduler::UpdateServerConnectionManagerStatus(
    HttpResponse::ServerConnectionCode code) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "New server connection code: "
            << HttpResponse::GetServerConnectionCodeString(code);

  connection_code_ = code;
}

void SyncScheduler::Start(Mode mode, const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  std::string thread_name = MessageLoop::current()->thread_name();
  if (thread_name.empty())
    thread_name = "<Main thread>";
  SDVLOG(2) << "Start called from thread "
            << thread_name << " with mode " << GetModeString(mode);
  if (!started_) {
    started_ = true;
    PostTask(FROM_HERE, "SendInitialSnapshot",
             base::Bind(&SyncScheduler::SendInitialSnapshot,
                        weak_ptr_factory_.GetWeakPtr()));
  }
  PostTask(FROM_HERE, "StartImpl",
           base::Bind(&SyncScheduler::StartImpl,
                      weak_ptr_factory_.GetWeakPtr(), mode, callback));
}

void SyncScheduler::SendInitialSnapshot() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  scoped_ptr<SyncSession> dummy(new SyncSession(session_context_, this,
      SyncSourceInfo(), ModelSafeRoutingInfo(),
      std::vector<ModelSafeWorker*>()));
  SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
  event.snapshot = dummy->TakeSnapshot();
  session_context_->NotifyListeners(event);
}

void SyncScheduler::StartImpl(Mode mode, const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "In StartImpl with mode " << GetModeString(mode);

  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(!session_context_->account_name().empty());
  DCHECK(syncer_.get());
  Mode old_mode = mode_;
  mode_ = mode;
  AdjustPolling(NULL);  // Will kick start poll timer if needed.
  if (!callback.is_null())
    callback.Run();

  if (old_mode != mode_) {
    // We just changed our mode. See if there are any pending jobs that we could
    // execute in the new mode.
    DoPendingJobIfPossible(false);
  }
}

SyncScheduler::JobProcessDecision SyncScheduler::DecideWhileInWaitInterval(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(wait_interval_.get());
  DCHECK_NE(job.purpose, SyncSessionJob::CLEAR_USER_DATA);
  DCHECK_NE(job.purpose, SyncSessionJob::CLEANUP_DISABLED_TYPES);

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

SyncScheduler::JobProcessDecision SyncScheduler::DecideOnJob(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (job.purpose == SyncSessionJob::CLEAR_USER_DATA ||
      job.purpose == SyncSessionJob::CLEANUP_DISABLED_TYPES)
    return CONTINUE;

  // See if our type is throttled.
  syncable::ModelTypeSet throttled_types =
      session_context_->GetThrottledTypes();
  if (job.purpose == SyncSessionJob::NUDGE &&
      job.session->source().updates_source == GetUpdatesCallerInfo::LOCAL) {
    syncable::ModelTypeSet requested_types;
    for (ModelTypePayloadMap::const_iterator i =
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

  // Freshness condition
  if (job.scheduled_start < last_sync_session_end_time_) {
    SDVLOG(2) << "Dropping job because of freshness";
    return DROP;
  }

  if (!session_context_->connection_manager()->HasInvalidAuthToken())
    return CONTINUE;

  SDVLOG(2) << "No valid auth token. Using that to decide on job.";
  return job.purpose == SyncSessionJob::NUDGE ? SAVE : DROP;
}

void SyncScheduler::InitOrCoalescePendingJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(job.purpose != SyncSessionJob::CONFIGURATION);
  if (pending_nudge_.get() == NULL) {
    SDVLOG(2) << "Creating a pending nudge job";
    SyncSession* s = job.session.get();
    scoped_ptr<SyncSession> session(new SyncSession(s->context(),
        s->delegate(), s->source(), s->routing_info(), s->workers()));

    SyncSessionJob new_job(SyncSessionJob::NUDGE, job.scheduled_start,
        make_linked_ptr(session.release()), false, job.from_here);
    pending_nudge_.reset(new SyncSessionJob(new_job));

    return;
  }

  SDVLOG(2) << "Coalescing a pending nudge";
  pending_nudge_->session->Coalesce(*(job.session.get()));
  pending_nudge_->scheduled_start = job.scheduled_start;

  // Unfortunately the nudge location cannot be modified. So it stores the
  // location of the first caller.
}

bool SyncScheduler::ShouldRunJob(const SyncSessionJob& job) {
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

void SyncScheduler::SaveJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK_NE(job.purpose, SyncSessionJob::CLEAR_USER_DATA);
  // TODO(sync): Should we also check that job.purpose !=
  // CLEANUP_DISABLED_TYPES?  (See http://crbug.com/90868.)
  if (job.purpose == SyncSessionJob::NUDGE) {
    SDVLOG(2) << "Saving a nudge job";
    InitOrCoalescePendingJob(job);
  } else if (job.purpose == SyncSessionJob::CONFIGURATION){
    SDVLOG(2) << "Saving a configuration job";
    DCHECK(wait_interval_.get());
    DCHECK(mode_ == CONFIGURATION_MODE);

    SyncSession* old = job.session.get();
    SyncSession* s(new SyncSession(session_context_, this, old->source(),
                                   old->routing_info(), old->workers()));
    SyncSessionJob new_job(job.purpose, TimeTicks::Now(),
                           make_linked_ptr(s), false, job.from_here);
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

void SyncScheduler::ScheduleClearUserData() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  PostTask(FROM_HERE, "ScheduleClearUserDataImpl",
           base::Bind(&SyncScheduler::ScheduleClearUserDataImpl,
                      weak_ptr_factory_.GetWeakPtr()));
}

// TODO(sync): Remove the *Impl methods for the other Schedule*
// functions, too.
void SyncScheduler::ScheduleCleanupDisabledTypes() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SyncSessionJob job(SyncSessionJob::CLEANUP_DISABLED_TYPES, TimeTicks::Now(),
                     make_linked_ptr(CreateSyncSession(SyncSourceInfo())),
                     false,
                     FROM_HERE);
  ScheduleSyncSessionJob(job);
}

void SyncScheduler::ScheduleNudge(
    const TimeDelta& delay,
    NudgeSource source, ModelTypeSet types,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay " << delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "types " << ModelTypeSetToString(types);

  ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromEnumSet(types, std::string());
  PostTask(nudge_location, "ScheduleNudgeImpl",
           base::Bind(&SyncScheduler::ScheduleNudgeImpl,
                      weak_ptr_factory_.GetWeakPtr(),
                      delay,
                      GetUpdatesFromNudgeSource(source),
                      types_with_payloads,
                      false,
                      nudge_location));
}

void SyncScheduler::ScheduleNudgeWithPayloads(
    const TimeDelta& delay,
    NudgeSource source, const ModelTypePayloadMap& types_with_payloads,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay " << delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "payloads "
      << syncable::ModelTypePayloadMapToString(types_with_payloads);

  PostTask(nudge_location, "ScheduleNudgeImpl",
           base::Bind(&SyncScheduler::ScheduleNudgeImpl,
                      weak_ptr_factory_.GetWeakPtr(),
                      delay,
                      GetUpdatesFromNudgeSource(source),
                      types_with_payloads,
                      false,
                      nudge_location));
}

void SyncScheduler::ScheduleClearUserDataImpl() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SyncSessionJob job(SyncSessionJob::CLEAR_USER_DATA, TimeTicks::Now(),
                     make_linked_ptr(CreateSyncSession(SyncSourceInfo())),
                     false,
                     FROM_HERE);

  ScheduleSyncSessionJob(job);
}

void SyncScheduler::ScheduleNudgeImpl(
    const TimeDelta& delay,
    GetUpdatesCallerInfo::GetUpdatesSource source,
    const ModelTypePayloadMap& types_with_payloads,
    bool is_canary_job, const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  SDVLOG_LOC(nudge_location, 2)
      << "In ScheduleNudgeImpl with delay "
      << delay.InMilliseconds() << " ms, "
      << "source " << GetUpdatesSourceString(source) << ", "
      << "payloads "
      << syncable::ModelTypePayloadMapToString(types_with_payloads)
      << (is_canary_job ? " (canary)" : "");

  SyncSourceInfo info(source, types_with_payloads);

  SyncSession* session(CreateSyncSession(info));
  SyncSessionJob job(SyncSessionJob::NUDGE, TimeTicks::Now() + delay,
                     make_linked_ptr(session), is_canary_job,
                     nudge_location);

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

// Helper to extract the routing info and workers corresponding to types in
// |types| from |current_routes| and |current_workers|.
void GetModelSafeParamsForTypes(ModelTypeSet types,
    const ModelSafeRoutingInfo& current_routes,
    const std::vector<ModelSafeWorker*>& current_workers,
    ModelSafeRoutingInfo* result_routes,
    std::vector<ModelSafeWorker*>* result_workers) {
  bool passive_group_added = false;

  typedef std::vector<ModelSafeWorker*>::const_iterator iter;
  for (ModelTypeSet::Iterator it = types.First();
       it.Good(); it.Inc()) {
    const syncable::ModelType t = it.Get();
    ModelSafeRoutingInfo::const_iterator route = current_routes.find(t);
    DCHECK(route != current_routes.end());
    ModelSafeGroup group = route->second;

    (*result_routes)[t] = group;
    iter w_tmp_it = std::find_if(current_workers.begin(), current_workers.end(),
                                 ModelSafeWorkerGroupIs(group));
    if (w_tmp_it != current_workers.end()) {
      iter result_workers_it = std::find_if(
          result_workers->begin(), result_workers->end(),
          ModelSafeWorkerGroupIs(group));
      if (result_workers_it == result_workers->end())
        result_workers->push_back(*w_tmp_it);

      if (group == GROUP_PASSIVE)
        passive_group_added = true;
    } else {
        NOTREACHED();
    }
  }

  // Always add group passive.
  if (passive_group_added == false) {
    iter it = std::find_if(current_workers.begin(), current_workers.end(),
                           ModelSafeWorkerGroupIs(GROUP_PASSIVE));
    if (it != current_workers.end())
      result_workers->push_back(*it);
    else
      NOTREACHED();
  }
}

void SyncScheduler::ScheduleConfig(
    ModelTypeSet types,
    GetUpdatesCallerInfo::GetUpdatesSource source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(IsConfigRelatedUpdateSourceValue(source));
  SDVLOG(2) << "Scheduling a config";

  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  GetModelSafeParamsForTypes(types,
                             session_context_->routing_info(),
                             session_context_->workers(),
                             &routes, &workers);

  PostTask(FROM_HERE, "ScheduleConfigImpl",
           base::Bind(&SyncScheduler::ScheduleConfigImpl,
                      weak_ptr_factory_.GetWeakPtr(),
                      routes,
                      workers,
                      source));
}

void SyncScheduler::ScheduleConfigImpl(
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers,
    const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  SDVLOG(2) << "In ScheduleConfigImpl";
  // TODO(tim): config-specific GetUpdatesCallerInfo value?
  SyncSession* session = new SyncSession(session_context_, this,
      SyncSourceInfo(source,
          syncable::ModelTypePayloadMapFromRoutingInfo(
              routing_info, std::string())),
      routing_info, workers);
  SyncSessionJob job(SyncSessionJob::CONFIGURATION, TimeTicks::Now(),
                     make_linked_ptr(session),
                     false,
                     FROM_HERE);
  ScheduleSyncSessionJob(job);
}

const char* SyncScheduler::GetModeString(SyncScheduler::Mode mode) {
  switch (mode) {
    ENUM_CASE(CONFIGURATION_MODE);
    ENUM_CASE(NORMAL_MODE);
  }
  return "";
}

const char* SyncScheduler::GetDecisionString(
    SyncScheduler::JobProcessDecision mode) {
  switch (mode) {
    ENUM_CASE(CONTINUE);
    ENUM_CASE(SAVE);
    ENUM_CASE(DROP);
  }
  return "";
}

// static
void SyncScheduler::SetSyncerStepsForPurpose(
    SyncSessionJob::SyncSessionJobPurpose purpose,
    SyncerStep* start,
    SyncerStep* end) {
  switch (purpose) {
    case SyncSessionJob::CONFIGURATION:
      *start = DOWNLOAD_UPDATES;
      *end = APPLY_UPDATES;
      return;
    case SyncSessionJob::CLEAR_USER_DATA:
      *start = CLEAR_PRIVATE_DATA;
      *end = CLEAR_PRIVATE_DATA;
       return;
    case SyncSessionJob::NUDGE:
    case SyncSessionJob::POLL:
      *start = SYNCER_BEGIN;
      *end = SYNCER_END;
      return;
    case SyncSessionJob::CLEANUP_DISABLED_TYPES:
      *start = CLEANUP_DISABLED_TYPES;
      *end = CLEANUP_DISABLED_TYPES;
      return;
    default:
      NOTREACHED();
      *start = SYNCER_END;
      *end = SYNCER_END;
      return;
  }
}

void SyncScheduler::PostTask(
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

void SyncScheduler::PostDelayedTask(
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

void SyncScheduler::ScheduleSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  TimeDelta delay = job.scheduled_start - TimeTicks::Now();
  if (delay < TimeDelta::FromMilliseconds(0))
    delay = TimeDelta::FromMilliseconds(0);
  SDVLOG_LOC(job.from_here, 2)
      << "In ScheduleSyncSessionJob with "
      << SyncSessionJob::GetPurposeString(job.purpose)
      << " job and " << delay.InMilliseconds() << " ms delay";

  if (job.purpose == SyncSessionJob::NUDGE) {
    SDVLOG_LOC(job.from_here, 2) << "Resetting pending_nudge";
    DCHECK(!pending_nudge_.get() || pending_nudge_->session.get() ==
           job.session);
    pending_nudge_.reset(new SyncSessionJob(job));
  }
  PostDelayedTask(job.from_here, "DoSyncSessionJob",
                  base::Bind(&SyncScheduler::DoSyncSessionJob,
                             weak_ptr_factory_.GetWeakPtr(),
                             job),
                  delay);
}

void SyncScheduler::DoSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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

void SyncScheduler::UpdateCarryoverSessionState(
    const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (old_job.purpose == SyncSessionJob::CONFIGURATION) {
    // Whatever types were part of a configuration task will have had updates
    // downloaded.  For that reason, we make sure they get recorded in the
    // event that they get disabled at a later time.
    ModelSafeRoutingInfo r(session_context_->previous_session_routing_info());
    if (!r.empty()) {
      ModelSafeRoutingInfo temp_r;
      ModelSafeRoutingInfo old_info(old_job.session->routing_info());
      std::set_union(r.begin(), r.end(), old_info.begin(), old_info.end(),
          std::insert_iterator<ModelSafeRoutingInfo>(temp_r, temp_r.begin()));
      session_context_->set_previous_session_routing_info(temp_r);
    }
  } else {
    session_context_->set_previous_session_routing_info(
        old_job.session->routing_info());
  }
}

void SyncScheduler::FinishSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // Update timing information for how often datatypes are triggering nudges.
  base::TimeTicks now = TimeTicks::Now();
  if (!last_sync_session_end_time_.is_null()) {
    ModelTypePayloadMap::const_iterator iter;
    for (iter = job.session->source().types.begin();
         iter != job.session->source().types.end();
         ++iter) {
#define PER_DATA_TYPE_MACRO(type_str) \
    SYNC_FREQ_HISTOGRAM("Sync.Freq" type_str, \
                        now - last_sync_session_end_time_);
      SYNC_DATA_TYPE_HISTOGRAM(iter->first);
#undef PER_DATA_TYPE_MACRO
    }
  }
  last_sync_session_end_time_ = now;

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

  UpdateCarryoverSessionState(job);
  if (IsSyncingCurrentlySilenced()) {
    SDVLOG(2) << "We are currently throttled; not scheduling the next sync.";
    // TODO(sync): Investigate whether we need to check job.purpose
    // here; see DCHECKs in SaveJob().  (See http://crbug.com/90868.)
    SaveJob(job);
    return;  // Nothing to do.
  }

  SDVLOG(2) << "Updating the next polling time after SyncMain";
  ScheduleNextSync(job);
}

void SyncScheduler::ScheduleNextSync(const SyncSessionJob& old_job) {
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

void SyncScheduler::AdjustPolling(const SyncSessionJob* old_job) {
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
  poll_timer_.Start(FROM_HERE, poll, this, &SyncScheduler::PollTimerCallback);
}

void SyncScheduler::RestartWaiting() {
  CHECK(wait_interval_.get());
  wait_interval_->timer.Stop();
  wait_interval_->timer.Start(FROM_HERE, wait_interval_->length,
                              this, &SyncScheduler::DoCanaryJob);
}

void SyncScheduler::HandleContinuationError(
    const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (DCHECK_IS_ON()) {
    if (IsBackingOff()) {
      DCHECK(wait_interval_->timer.IsRunning() || old_job.is_canary_job);
    }
  }

  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length : TimeDelta::FromSeconds(1));

  SDVLOG(2) << "In handle continuation error with "
            << SyncSessionJob::GetPurposeString(old_job.purpose)
            << " job. The time delta(ms) is "
            << length.InMilliseconds();

  // This will reset the had_nudge variable as well.
  wait_interval_.reset(new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF,
                                        length));
  if (old_job.purpose == SyncSessionJob::CONFIGURATION) {
    SyncSession* old = old_job.session.get();
    SyncSession* s(new SyncSession(session_context_, this,
        old->source(), old->routing_info(), old->workers()));
    SyncSessionJob job(old_job.purpose, TimeTicks::Now() + length,
                        make_linked_ptr(s), false, FROM_HERE);
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

// static
TimeDelta SyncScheduler::GetRecommendedDelay(const TimeDelta& last_delay) {
  if (last_delay.InSeconds() >= kMaxBackoffSeconds)
    return TimeDelta::FromSeconds(kMaxBackoffSeconds);

  // This calculates approx. base_delay_seconds * 2 +/- base_delay_seconds / 2
  int64 backoff_s =
      std::max(static_cast<int64>(1),
               last_delay.InSeconds() * kBackoffRandomizationFactor);

  // Flip a coin to randomize backoff interval by +/- 50%.
  int rand_sign = base::RandInt(0, 1) * 2 - 1;

  // Truncation is adequate for rounding here.
  backoff_s = backoff_s +
      (rand_sign * (last_delay.InSeconds() / kBackoffRandomizationFactor));

  // Cap the backoff interval.
  backoff_s = std::max(static_cast<int64>(1),
                       std::min(backoff_s, kMaxBackoffSeconds));

  return TimeDelta::FromSeconds(backoff_s);
}

void SyncScheduler::RequestStop(const base::Closure& callback) {
  syncer_->RequestEarlyExit();  // Safe to call from any thread.
  DCHECK(weak_handle_this_.IsInitialized());
  SDVLOG(3) << "Posting StopImpl";
  weak_handle_this_.Call(FROM_HERE,
                         &SyncScheduler::StopImpl,
                         callback);
}

void SyncScheduler::StopImpl(const base::Closure& callback) {
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

void SyncScheduler::DoCanaryJob() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "Do canary job";
  DoPendingJobIfPossible(true);
}

void SyncScheduler::DoPendingJobIfPossible(bool is_canary_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SyncSessionJob* job_to_execute = NULL;
  if (mode_ == CONFIGURATION_MODE && wait_interval_.get()
      && wait_interval_->pending_configure_job.get()) {
    SDVLOG(2) << "Found pending configure job";
    job_to_execute = wait_interval_->pending_configure_job.get();
  } else if (mode_ == NORMAL_MODE && pending_nudge_.get()) {
    SDVLOG(2) << "Found pending nudge job";
    // Pending jobs mostly have time from the past. Reset it so this job
    // will get executed.
    if (pending_nudge_->scheduled_start < TimeTicks::Now())
      pending_nudge_->scheduled_start = TimeTicks::Now();

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

SyncSession* SyncScheduler::CreateSyncSession(const SyncSourceInfo& source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DVLOG(2) << "Creating sync session with routes "
           << ModelSafeRoutingInfoToString(session_context_->routing_info());

  SyncSourceInfo info(source);
  SyncSession* session(new SyncSession(session_context_, this, info,
      session_context_->routing_info(), session_context_->workers()));

  return session;
}

void SyncScheduler::PollTimerCallback() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ModelSafeRoutingInfo r;
  ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromRoutingInfo(r, std::string());
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, types_with_payloads);
  SyncSession* s = CreateSyncSession(info);

  SyncSessionJob job(SyncSessionJob::POLL, TimeTicks::Now(),
                     make_linked_ptr(s),
                     false,
                     FROM_HERE);

  ScheduleSyncSessionJob(job);
}

void SyncScheduler::Unthrottle() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);
  SDVLOG(2) << "Unthrottled.";
  DoCanaryJob();
  wait_interval_.reset();
}

void SyncScheduler::Notify(SyncEngineEvent::EventCause cause) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->NotifyListeners(SyncEngineEvent(cause));
}

bool SyncScheduler::IsBackingOff() const {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::EXPONENTIAL_BACKOFF;
}

void SyncScheduler::OnSilencedUntil(const base::TimeTicks& silenced_until) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  wait_interval_.reset(new WaitInterval(WaitInterval::THROTTLED,
                                        silenced_until - TimeTicks::Now()));
  wait_interval_->timer.Start(FROM_HERE, wait_interval_->length, this,
      &SyncScheduler::Unthrottle);
}

bool SyncScheduler::IsSyncingCurrentlySilenced() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::THROTTLED;
}

void SyncScheduler::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  syncer_short_poll_interval_seconds_ = new_interval;
}

void SyncScheduler::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  syncer_long_poll_interval_seconds_ = new_interval;
}

void SyncScheduler::OnReceivedSessionsCommitDelay(
    const base::TimeDelta& new_delay) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sessions_commit_delay_ = new_delay;
}

void SyncScheduler::OnShouldStopSyncingPermanently() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "OnShouldStopSyncingPermanently";
  syncer_->RequestEarlyExit();  // Thread-safe.
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncScheduler::OnActionableError(
    const sessions::SyncSessionSnapshot& snap) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SDVLOG(2) << "OnActionableError";
  SyncEngineEvent event(SyncEngineEvent::ACTIONABLE_ERROR);
  event.snapshot = snap;
  session_context_->NotifyListeners(event);
}

void SyncScheduler::OnSyncProtocolError(
    const sessions::SyncSessionSnapshot& snapshot) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (ShouldRequestEarlyExit(snapshot.errors().sync_protocol_error)) {
    SDVLOG(2) << "Sync Scheduler requesting early exit.";
    syncer_->RequestEarlyExit();  // Thread-safe.
  }
  if (IsActionableError(snapshot.errors().sync_protocol_error))
    OnActionableError(snapshot);
}

void SyncScheduler::set_notifications_enabled(bool notifications_enabled) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->set_notifications_enabled(notifications_enabled);
}

base::TimeDelta SyncScheduler::sessions_commit_delay() const {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return sessions_commit_delay_;
}

#undef SDVLOG_LOC

#undef SDVLOG

#undef SLOG

#undef ENUM_CASE

}  // browser_sync
