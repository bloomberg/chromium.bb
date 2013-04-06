// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_session_job.h"
#include "sync/internal_api/public/sessions/model_neutral_state.h"

namespace syncer {

SyncSessionJob::~SyncSessionJob() {
}

SyncSessionJob::SyncSessionJob(
    Purpose purpose,
    base::TimeTicks start,
    const sessions::SyncSourceInfo& source_info,
    const ConfigurationParams& config_params)
    : purpose_(purpose),
      source_info_(source_info),
      scheduled_start_(start),
      config_params_(config_params) {
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

bool SyncSessionJob::Finish(bool early_exit, sessions::SyncSession* session) {
  // Did we quit part-way through due to premature exit condition, like
  // shutdown?  Note that this branch will not be hit for other kinds
  // of early return scenarios, like certain kinds of transient errors.
  if (early_exit)
    return false;

  // Did we hit any errors along the way?
  if (sessions::HasSyncerError(
      session->status_controller().model_neutral_state())) {
    return false;
  }

  const sessions::ModelNeutralState& state(
      session->status_controller().model_neutral_state());
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

void SyncSessionJob::CoalesceSources(const sessions::SyncSourceInfo& source) {
  CoalesceStates(source.types, &source_info_.types);
  source_info_.updates_source = source.updates_source;
}

SyncSessionJob::Purpose SyncSessionJob::purpose() const {
  return purpose_;
}

const sessions::SyncSourceInfo& SyncSessionJob::source_info() const {
  return source_info_;
}

base::TimeTicks SyncSessionJob::scheduled_start() const {
  return scheduled_start_;
}

void SyncSessionJob::set_scheduled_start(base::TimeTicks start) {
  scheduled_start_ = start;
};

ConfigurationParams SyncSessionJob::config_params() const {
  return config_params_;
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
