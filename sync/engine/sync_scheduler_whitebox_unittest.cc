// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/time.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/sessions/test_util.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/mock_connection_manager.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/fake_extensions_activity_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {
using browser_sync::Syncer;
using sessions::SyncSession;
using sessions::SyncSessionContext;
using sessions::SyncSourceInfo;
using sync_pb::GetUpdatesCallerInfo;

class SyncSchedulerWhiteboxTest : public testing::Test {
 public:
  virtual void SetUp() {
    dir_maker_.SetUp();
    Syncer* syncer = new Syncer();

    ModelSafeRoutingInfo routes;
    routes[syncable::BOOKMARKS] = GROUP_UI;
    routes[syncable::NIGORI] = GROUP_PASSIVE;

    workers_.push_back(make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers_.push_back(make_scoped_refptr(new FakeModelWorker(GROUP_PASSIVE)));

    std::vector<ModelSafeWorker*> workers;
    for (std::vector<scoped_refptr<FakeModelWorker> >::iterator it =
         workers_.begin(); it != workers_.end(); ++it) {
      workers.push_back(it->get());
    }

    connection_.reset(new MockConnectionManager(NULL));
    throttled_data_type_tracker_.reset(new ThrottledDataTypeTracker(NULL));
    context_.reset(
        new SyncSessionContext(
            connection_.get(), dir_maker_.directory(),
            routes, workers, &extensions_activity_monitor_,
            throttled_data_type_tracker_.get(),
            std::vector<SyncEngineEventListener*>(), NULL, NULL));
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    scheduler_.reset(
        new SyncScheduler("TestSyncSchedulerWhitebox", context(), syncer));
  }

  virtual void TearDown() {
    scheduler_.reset();
  }

  void SetMode(SyncScheduler::Mode mode) {
    scheduler_->mode_ = mode;
  }

  void SetLastSyncedTime(base::TimeTicks ticks) {
    scheduler_->last_sync_session_end_time_ = ticks;
  }

  void ResetWaitInterval() {
    scheduler_->wait_interval_.reset();
  }

  void SetWaitIntervalToThrottled() {
    scheduler_->wait_interval_.reset(new SyncScheduler::WaitInterval(
        SyncScheduler::WaitInterval::THROTTLED, TimeDelta::FromSeconds(1)));
  }

  void SetWaitIntervalToExponentialBackoff() {
    scheduler_->wait_interval_.reset(
       new SyncScheduler::WaitInterval(
       SyncScheduler::WaitInterval::EXPONENTIAL_BACKOFF,
       TimeDelta::FromSeconds(1)));
  }

  void SetWaitIntervalHadNudge(bool had_nudge) {
    scheduler_->wait_interval_->had_nudge = had_nudge;
  }

  SyncScheduler::JobProcessDecision DecideOnJob(
      const SyncScheduler::SyncSessionJob& job) {
    return scheduler_->DecideOnJob(job);
  }

  void InitializeSyncerOnNormalMode() {
    SetMode(SyncScheduler::NORMAL_MODE);
    ResetWaitInterval();
    SetLastSyncedTime(base::TimeTicks::Now());
  }

  SyncScheduler::JobProcessDecision CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::SyncSessionJobPurpose purpose) {
    SyncSession* s = scheduler_->CreateSyncSession(SyncSourceInfo());
    SyncScheduler::SyncSessionJob job(purpose, TimeTicks::Now(),
         make_linked_ptr(s),
         false,
         ConfigurationParams(),
         FROM_HERE);
    return DecideOnJob(job);
  }

  SyncSessionContext* context() { return context_.get(); }

 private:
  MessageLoop message_loop_;
  scoped_ptr<MockConnectionManager> connection_;
  scoped_ptr<SyncSessionContext> context_;
  std::vector<scoped_refptr<FakeModelWorker> > workers_;
  FakeExtensionsActivityMonitor extensions_activity_monitor_;
  scoped_ptr<ThrottledDataTypeTracker> throttled_data_type_tracker_;
  TestDirectorySetterUpper dir_maker_;

 protected:
  // Declared here to ensure it is destructed before the objects it references.
  scoped_ptr<SyncScheduler> scheduler_;
};

TEST_F(SyncSchedulerWhiteboxTest, SaveNudge) {
  InitializeSyncerOnNormalMode();

  // Now set the mode to configure.
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SyncScheduler::JobProcessDecision decision =
      CreateAndDecideJob(SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest, SaveNudgeWhileTypeThrottled) {
  InitializeSyncerOnNormalMode();

  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);

  // Mark bookmarks as throttled.
  context()->throttled_data_type_tracker()->SetUnthrottleTime(
      types, base::TimeTicks::Now() + base::TimeDelta::FromHours(2));

  syncable::ModelTypePayloadMap types_with_payload;
  types_with_payload[syncable::BOOKMARKS] = "";

  SyncSourceInfo info(GetUpdatesCallerInfo::LOCAL, types_with_payload);
  SyncSession* s = scheduler_->CreateSyncSession(info);

  // Now schedule a nudge with just bookmarks and the change is local.
  SyncScheduler::SyncSessionJob job(SyncScheduler::SyncSessionJob::NUDGE,
                                    TimeTicks::Now(),
                                    make_linked_ptr(s),
                                    false,
                                    ConfigurationParams(),
                                    FROM_HERE);

  SyncScheduler::JobProcessDecision decision = DecideOnJob(job);
  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueNudge) {
  InitializeSyncerOnNormalMode();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, DropPoll) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::POLL);

  EXPECT_EQ(decision, SyncScheduler::DROP);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinuePoll) {
  InitializeSyncerOnNormalMode();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::POLL);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueConfiguration) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CONFIGURATION);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, SaveConfigurationWhileThrottled) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SetWaitIntervalToThrottled();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CONFIGURATION);

  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest, SaveNudgeWhileThrottled) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SetWaitIntervalToThrottled();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest,
       ContinueClearUserDataUnderAllCircumstances) {
  InitializeSyncerOnNormalMode();

  SetMode(SyncScheduler::CONFIGURATION_MODE);
  SetWaitIntervalToThrottled();
  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CLEAR_USER_DATA);
  EXPECT_EQ(decision, SyncScheduler::CONTINUE);

  SetMode(SyncScheduler::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();
  decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CLEAR_USER_DATA);
  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueNudgeWhileExponentialBackOff) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, DropNudgeWhileExponentialBackOff) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();
  SetWaitIntervalHadNudge(true);

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::DROP);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueCanaryJobConfig) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);
  SetWaitIntervalToExponentialBackoff();

  struct SyncScheduler::SyncSessionJob job;
  job.purpose = SyncScheduler::SyncSessionJob::CONFIGURATION;
  job.scheduled_start = TimeTicks::Now();
  job.is_canary_job = true;
  SyncScheduler::JobProcessDecision decision = DecideOnJob(job);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

}  // namespace browser_sync
