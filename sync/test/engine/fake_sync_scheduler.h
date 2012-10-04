// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of the SyncScheduler. If needed, we should add default
// logic needed for tests (invoking callbacks, etc) here rather than in higher
// level test classes.

#ifndef SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_
#define SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_

#include "base/message_loop.h"
#include "sync/engine/sync_scheduler.h"

namespace syncer {

class FakeSyncScheduler : public SyncScheduler {
 public:
  FakeSyncScheduler();
  virtual ~FakeSyncScheduler();

  virtual void Start(Mode mode) OVERRIDE;
  virtual void RequestStop(const base::Closure& callback) OVERRIDE;
  virtual void ScheduleNudgeAsync(
      const base::TimeDelta& delay,
      NudgeSource source,
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual void ScheduleNudgeWithStatesAsync(
      const base::TimeDelta& delay, NudgeSource source,
      const ModelTypeInvalidationMap& invalidation_map,
      const tracked_objects::Location& nudge_location) OVERRIDE;
  virtual bool ScheduleConfiguration(
      const ConfigurationParams& params) OVERRIDE;
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
    MessageLoop* const created_on_loop_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_
