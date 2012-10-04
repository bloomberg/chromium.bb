// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/fake_sync_scheduler.h"

namespace syncer {

FakeSyncScheduler::FakeSyncScheduler()
    : created_on_loop_(MessageLoop::current()) {}

FakeSyncScheduler::~FakeSyncScheduler() {}

void FakeSyncScheduler::Start(Mode mode) {
}

void FakeSyncScheduler::RequestStop(const base::Closure& callback) {
  created_on_loop_->PostTask(FROM_HERE, callback);
}

void FakeSyncScheduler::ScheduleNudgeAsync(
     const base::TimeDelta& delay,
     NudgeSource source,
     ModelTypeSet types,
     const tracked_objects::Location& nudge_location) {
}

void FakeSyncScheduler::ScheduleNudgeWithStatesAsync(
     const base::TimeDelta& delay, NudgeSource source,
     const ModelTypeInvalidationMap& invalidation_map,
     const tracked_objects::Location& nudge_location) {
}

bool FakeSyncScheduler::ScheduleConfiguration(
     const ConfigurationParams& params) {
  params.ready_task.Run();
  return true;
}

void FakeSyncScheduler::SetNotificationsEnabled(bool notifications_enabled) {
}

base::TimeDelta FakeSyncScheduler::GetSessionsCommitDelay() const {
  return base::TimeDelta();
}

void FakeSyncScheduler::OnCredentialsUpdated() {

}

void FakeSyncScheduler::OnConnectionStatusChange() {

}

void FakeSyncScheduler::OnSilencedUntil(
     const base::TimeTicks& silenced_until) {
}
bool FakeSyncScheduler::IsSyncingCurrentlySilenced() {
  return false;
}

void FakeSyncScheduler::OnReceivedShortPollIntervalUpdate(
     const base::TimeDelta& new_interval) {
}

void FakeSyncScheduler::OnReceivedLongPollIntervalUpdate(
     const base::TimeDelta& new_interval) {
}

void FakeSyncScheduler::OnReceivedSessionsCommitDelay(
     const base::TimeDelta& new_delay) {
}

void FakeSyncScheduler::OnShouldStopSyncingPermanently() {
}

void FakeSyncScheduler::OnSyncProtocolError(
     const sessions::SyncSessionSnapshot& snapshot) {
}

}  // namespace syncer
