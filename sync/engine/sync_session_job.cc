// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_session_job.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"

namespace syncer {

SyncSessionJob::~SyncSessionJob() {
  if (destruction_observer_)
    destruction_observer_->OnJobDestroyed(this);
}

SyncSessionJob::SyncSessionJob(
    Purpose purpose,
    base::TimeTicks start,
    scoped_ptr<sessions::SyncSession> session,
    const ConfigurationParams& config_params)
    : purpose_(purpose),
      scheduled_start_(start),
      session_(session.Pass()),
      is_canary_(false),
      config_params_(config_params),
      finished_(NOT_FINISHED) {
}

void SyncSessionJob::set_destruction_observer(
    const base::WeakPtr<DestructionObserver>& destruction_observer) {
  destruction_observer_ = destruction_observer;
}

#define ENUM_CASE(x) case x: return #x; break;
const char* SyncSessionJob::GetPurposeString(SyncSessionJob::Purpose purpose) {
  switch (purpose) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(POLL);
    ENUM_CASE(NUDGE);
    ENUM_CASE(CONFIGURATION);
  }
  NOTREACHED();
  return "";
}
#undef ENUM_CASE

bool SyncSessionJob::Finish(bool early_exit) {
  DCHECK_EQ(finished_, NOT_FINISHED);
  // Did we run through all SyncerSteps from start_step() to end_step()
  // until the SyncSession returned !HasMoreToSync()?
  // Note: if not, it's possible the scheduler hasn't started with
  // SyncShare yet, it's possible there is still more to sync in the session,
  // and it's also possible the job quit part way through due to a premature
  // exit condition (such as shutdown).
  finished_ = early_exit ? EARLY_EXIT : FINISHED;

  if (early_exit)
    return false;

  // Did we hit any errors along the way?
  if (sessions::HasSyncerError(
      session_->status_controller().model_neutral_state())) {
    return false;
  }

  const sessions::ModelNeutralState& state(
      session_->status_controller().model_neutral_state());
  switch (purpose_) {
    case POLL:
    case NUDGE:
      DCHECK_NE(state.last_download_updates_result, UNSET);
      DCHECK_NE(state.commit_result, UNSET);
      break;
    case CONFIGURATION:
      DCHECK_NE(state.last_download_updates_result, UNSET);
      break;
    case UNKNOWN:
    default:
      NOTREACHED();
  }

  if (!config_params_.ready_task.is_null())
    config_params_.ready_task.Run();
  return true;
}

scoped_ptr<SyncSessionJob> SyncSessionJob::CloneAndAbandon() {
  DCHECK_EQ(finished_, NOT_FINISHED);
  // Clone |this|, and abandon it by NULL-ing session_.
  return scoped_ptr<SyncSessionJob> (new SyncSessionJob(
      purpose_, scheduled_start_, session_.Pass(),
      config_params_));
}

scoped_ptr<SyncSessionJob> SyncSessionJob::Clone() const {
  DCHECK_GT(finished_, NOT_FINISHED);
  return scoped_ptr<SyncSessionJob>(new SyncSessionJob(
      purpose_, scheduled_start_, CloneSession().Pass(),
      config_params_));
}

scoped_ptr<sessions::SyncSession> SyncSessionJob::CloneSession() const {
  return scoped_ptr<sessions::SyncSession>(
      new sessions::SyncSession(session_->context(),
          session_->delegate(), session_->source()));
}

bool SyncSessionJob::is_canary() const {
  return is_canary_;
}

SyncSessionJob::Purpose SyncSessionJob::purpose() const {
  return purpose_;
}

base::TimeTicks SyncSessionJob::scheduled_start() const {
  return scheduled_start_;
}

void SyncSessionJob::set_scheduled_start(base::TimeTicks start) {
  scheduled_start_ = start;
};

const sessions::SyncSession* SyncSessionJob::session() const {
  return session_.get();
}

sessions::SyncSession* SyncSessionJob::mutable_session() {
  return session_.get();
}

ConfigurationParams SyncSessionJob::config_params() const {
  return config_params_;
}

void SyncSessionJob::GrantCanaryPrivilege() {
  DCHECK_EQ(finished_, NOT_FINISHED);
  DVLOG(2) << "Granting canary priviliege to " << session_.get();
  is_canary_ = true;
}

SyncerStep SyncSessionJob::start_step() const {
  SyncerStep start, end;
  GetSyncerStepsForPurpose(purpose_, &start, &end);
  return start;
}

SyncerStep SyncSessionJob::end_step() const {
  SyncerStep start, end;
  GetSyncerStepsForPurpose(purpose_, &start, &end);
  return end;
}

// static
void SyncSessionJob::GetSyncerStepsForPurpose(Purpose purpose,
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

}  // namespace syncer
