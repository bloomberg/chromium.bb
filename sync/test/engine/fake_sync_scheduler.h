// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of the SyncScheduler. If needed, we should add default
// logic needed for tests (invoking callbacks, etc) here rather than in higher
// level test classes.

#ifndef SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_
#define SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_

#include "base/message_loop/message_loop.h"
#include "sync/engine/sync_scheduler.h"

namespace syncer {

class FakeSyncScheduler : public SyncScheduler {
 public:
  FakeSyncScheduler();
  virtual ~FakeSyncScheduler();

  virtual void Start(Mode mode) override;
  virtual void Stop() override;
  virtual void ScheduleLocalNudge(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) override;
  virtual void ScheduleLocalRefreshRequest(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) override;
  virtual void ScheduleInvalidationNudge(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> interface,
      const tracked_objects::Location& nudge_location) override;
  virtual void ScheduleConfiguration(
      const ConfigurationParams& params) override;
  virtual void ScheduleInitialSyncNudge(syncer::ModelType model_type) override;
  virtual void SetNotificationsEnabled(bool notifications_enabled) override;

  virtual void OnCredentialsUpdated() override;
  virtual void OnConnectionStatusChange() override;

  // SyncSession::Delegate implementation.
  virtual void OnThrottled(
      const base::TimeDelta& throttle_duration) override;
  virtual void OnTypesThrottled(
      ModelTypeSet types,
      const base::TimeDelta& throttle_duration) override;
  virtual bool IsCurrentlyThrottled() override;
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  virtual void OnReceivedCustomNudgeDelays(
      const std::map<ModelType, base::TimeDelta>& nudge_delays) override;
  virtual void OnReceivedClientInvalidationHintBufferSize(int size) override;
  virtual void OnSyncProtocolError(
      const SyncProtocolError& error) override;
  virtual void OnReceivedGuRetryDelay(
      const base::TimeDelta& delay) override;
  virtual void OnReceivedMigrationRequest(ModelTypeSet types) override;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_
