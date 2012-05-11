// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer.h"
#include "sync/sessions/test_util.h"
#include "sync/test/engine/fake_model_safe_worker_registrar.h"
#include "sync/test/engine/mock_connection_manager.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/fake_extensions_activity_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;

namespace browser_sync {
using sessions::SyncSession;
using sessions::SyncSessionContext;
using sessions::SyncSessionSnapshot;
using syncable::ModelTypeSet;
using sync_pb::GetUpdatesCallerInfo;

class MockSyncer : public Syncer {
 public:
  MOCK_METHOD3(SyncShare, void(sessions::SyncSession*, SyncerStep,
                               SyncerStep));
};

// Used when tests want to record syncing activity to examine later.
struct SyncShareRecords {
  std::vector<TimeTicks> times;
  std::vector<SyncSessionSnapshot> snapshots;
};

void QuitLoopNow() {
  // We use QuitNow() instead of Quit() as the latter may get stalled
  // indefinitely in the presence of repeated timers with low delays
  // and a slow test (e.g., ThrottlingDoesThrottle [which has a poll
  // delay of 5ms] run under TSAN on the trybots).
  MessageLoop::current()->QuitNow();
}

void RunLoop() {
  MessageLoop::current()->Run();
}

void PumpLoop() {
  // Do it this way instead of RunAllPending to pump loop exactly once
  // (necessary in the presence of timers; see comment in
  // QuitLoopNow).
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&QuitLoopNow));
  RunLoop();
}

// Convenient to use in tests wishing to analyze SyncShare calls over time.
static const size_t kMinNumSamples = 5;
class SyncSchedulerTest : public testing::Test {
 public:
  SyncSchedulerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        context_(NULL),
        syncer_(NULL),
        delay_(NULL) {}

  class MockDelayProvider : public SyncScheduler::DelayProvider {
   public:
    MOCK_METHOD1(GetDelay, TimeDelta(const TimeDelta&));
  };

  virtual void SetUp() {
    dir_maker_.SetUp();
    syncer_ = new MockSyncer();
    delay_ = NULL;
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_UI;
    routing_info[syncable::AUTOFILL] = GROUP_DB;
    routing_info[syncable::THEMES] = GROUP_UI;
    routing_info[syncable::NIGORI] = GROUP_PASSIVE;
    registrar_.reset(new FakeModelSafeWorkerRegistrar(routing_info));
    connection_.reset(new MockConnectionManager(directory()));
    connection_->SetServerReachable();
    context_ = new SyncSessionContext(
        connection_.get(), directory(), registrar_.get(),
        &extensions_activity_monitor_,
        std::vector<SyncEngineEventListener*>(), NULL, NULL);
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    scheduler_.reset(
        new SyncScheduler("TestSyncScheduler", context_, syncer_));
  }

  SyncScheduler* scheduler() { return scheduler_.get(); }
  MockSyncer* syncer() { return syncer_; }
  MockDelayProvider* delay() { return delay_; }
  MockConnectionManager* connection() { return connection_.get(); }
  TimeDelta zero() { return TimeDelta::FromSeconds(0); }
  TimeDelta timeout() {
    return TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms());
  }

  virtual void TearDown() {
    PumpLoop();
    scheduler_.reset();
    PumpLoop();
    dir_maker_.TearDown();
  }

  void AnalyzePollRun(const SyncShareRecords& records, size_t min_num_samples,
      const TimeTicks& optimal_start, const TimeDelta& poll_interval) {
    const std::vector<TimeTicks>& data(records.times);
    EXPECT_GE(data.size(), min_num_samples);
    for (size_t i = 0; i < data.size(); i++) {
      SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
      TimeTicks optimal_next_sync = optimal_start + poll_interval * i;
      EXPECT_GE(data[i], optimal_next_sync);
      EXPECT_EQ(GetUpdatesCallerInfo::PERIODIC,
                records.snapshots[i].source().updates_source);
    }
  }

  void DoQuitLoopNow() {
    QuitLoopNow();
  }

  void StartSyncScheduler(SyncScheduler::Mode mode) {
    scheduler()->Start(
        mode,
        base::Bind(&SyncSchedulerTest::DoQuitLoopNow,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // This stops the scheduler synchronously.
  void StopSyncScheduler() {
    scheduler()->RequestStop(base::Bind(&SyncSchedulerTest::DoQuitLoopNow,
                             weak_ptr_factory_.GetWeakPtr()));
    RunLoop();
  }

  bool RunAndGetBackoff() {
    ModelTypeSet nudge_types;
    StartSyncScheduler(SyncScheduler::NORMAL_MODE);
    RunLoop();

    scheduler()->ScheduleNudge(
        zero(), NUDGE_SOURCE_LOCAL, nudge_types, FROM_HERE);
    RunLoop();

    return scheduler()->IsBackingOff();
  }

  void UseMockDelayProvider() {
    delay_ = new MockDelayProvider();
    scheduler_->delay_provider_.reset(delay_);
  }

  // Compare a ModelTypeSet to a ModelTypePayloadMap, ignoring
  // payload values.
  bool CompareModelTypeSetToModelTypePayloadMap(
      ModelTypeSet lhs,
      const syncable::ModelTypePayloadMap& rhs) {
    size_t count = 0;
    for (syncable::ModelTypePayloadMap::const_iterator i = rhs.begin();
         i != rhs.end(); ++i, ++count) {
      if (!lhs.Has(i->first))
        return false;
    }
    if (lhs.Size() != count)
      return false;
    return true;
  }

  SyncSessionContext* context() { return context_; }

 private:
  syncable::Directory* directory() {
    return dir_maker_.directory();
  }

  base::WeakPtrFactory<SyncSchedulerTest> weak_ptr_factory_;
  MessageLoop message_loop_;
  TestDirectorySetterUpper dir_maker_;
  scoped_ptr<SyncScheduler> scheduler_;
  scoped_ptr<MockConnectionManager> connection_;
  SyncSessionContext* context_;
  MockSyncer* syncer_;
  MockDelayProvider* delay_;
  scoped_ptr<FakeModelSafeWorkerRegistrar> registrar_;
  FakeExtensionsActivityMonitor extensions_activity_monitor_;
};

class BackoffTriggersSyncSchedulerTest : public SyncSchedulerTest {
  void SetUp() {
    SyncSchedulerTest::SetUp();
    UseMockDelayProvider();
    EXPECT_CALL(*delay(), GetDelay(_))
        .WillRepeatedly(Return(TimeDelta::FromMilliseconds(1)));
  }

  void TearDown() {
    StopSyncScheduler();
    SyncSchedulerTest::TearDown();
  }
};

void RecordSyncShareImpl(SyncSession* s, SyncShareRecords* record) {
  record->times.push_back(TimeTicks::Now());
  record->snapshots.push_back(s->TakeSnapshot());
}

ACTION_P(RecordSyncShare, record) {
  RecordSyncShareImpl(arg0, record);
  QuitLoopNow();
}

ACTION_P2(RecordSyncShareMultiple, record, quit_after) {
  RecordSyncShareImpl(arg0, record);
  EXPECT_LE(record->times.size(), quit_after);
  if (record->times.size() >= quit_after) {
    QuitLoopNow();
  }
}

ACTION(AddFailureAndQuitLoopNow) {
  ADD_FAILURE();
  QuitLoopNow();
}

ACTION(QuitLoopNowAction) {
  QuitLoopNow();
}

// Test nudge scheduling.
TEST_F(SyncSchedulerTest, Nudge) {
  SyncShareRecords records;
  ModelTypeSet model_types(syncable::BOOKMARKS);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))))
      .RetiresOnSaturation();

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types,
      records.snapshots[0].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[0].source().updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareRecords records2;
  model_types.Remove(syncable::BOOKMARKS);
  model_types.Put(syncable::AUTOFILL);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records2))));
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records2.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types,
      records2.snapshots[0].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records2.snapshots[0].source().updates_source);
}

// Make sure a regular config command is scheduled fine in the absence of any
// errors.
TEST_F(SyncSchedulerTest, Config) {
  SyncShareRecords records;
  const ModelTypeSet model_types(syncable::BOOKMARKS);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      model_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types,
      records.snapshots[0].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::RECONFIGURATION,
            records.snapshots[0].source().updates_source);
}

// Simulate a failure and make sure the config request is retried.
TEST_F(SyncSchedulerTest, ConfigWithBackingOff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(1)));
  SyncShareRecords records;
  const ModelTypeSet model_types(syncable::BOOKMARKS);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  ASSERT_EQ(0U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  RunLoop();

  ASSERT_EQ(2U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types,
      records.snapshots[1].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::RECONFIGURATION,
            records.snapshots[1].source().updates_source);
}

// Issue 2 config commands. Second one right after the first has failed
// and make sure LATEST is executed.
TEST_F(SyncSchedulerTest, MultipleConfigWithBackingOff) {
  const ModelTypeSet
      model_types1(syncable::BOOKMARKS),
      model_types2(syncable::AUTOFILL);
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(30)));
  SyncShareRecords records;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  ASSERT_EQ(0U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types1, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types2, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(2U, records.snapshots.size());
  RunLoop();

  ASSERT_EQ(3U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types2,
      records.snapshots[2].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::RECONFIGURATION,
            records.snapshots[2].source().updates_source);
}

// Issue a nudge when the config has failed. Make sure both the config and
// nudge are executed.
TEST_F(SyncSchedulerTest, NudgeWithConfigWithBackingOff) {
  const ModelTypeSet model_types(syncable::BOOKMARKS);
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(50)));
  SyncShareRecords records;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  ASSERT_EQ(0U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  ASSERT_EQ(2U, records.snapshots.size());
  RunLoop();

  // Now change the mode so nudge can execute.
  ASSERT_EQ(3U, records.snapshots.size());
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  ASSERT_EQ(4U, records.snapshots.size());

  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types,
      records.snapshots[2].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::RECONFIGURATION,
            records.snapshots[2].source().updates_source);

  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(model_types,
      records.snapshots[3].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[3].source().updates_source);

}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeCoalescing) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r))));
  const ModelTypeSet
      types1(syncable::BOOKMARKS),
      types2(syncable::AUTOFILL),
      types3(syncable::THEMES);
  TimeDelta delay = zero();
  TimeTicks optimal_time = TimeTicks::Now() + delay;
  scheduler()->ScheduleNudge(
      delay, NUDGE_SOURCE_UNKNOWN, types1, FROM_HERE);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types2, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r.snapshots.size());
  EXPECT_GE(r.times[0], optimal_time);
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(
      Union(types1, types2), r.snapshots[0].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[0].source().updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareRecords r2;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r2))));
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_NOTIFICATION, types3, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r2.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(types3,
      r2.snapshots[0].source().types));
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r2.snapshots[0].source().updates_source);
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeCoalescingWithDifferentTimings) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r))));
  syncable::ModelTypeSet types1(syncable::BOOKMARKS),
      types2(syncable::AUTOFILL), types3;

  // Create a huge time delay.
  TimeDelta delay = TimeDelta::FromDays(1);

  scheduler()->ScheduleNudge(
      delay, NUDGE_SOURCE_UNKNOWN, types1, FROM_HERE);

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_UNKNOWN, types2, FROM_HERE);

  TimeTicks min_time = TimeTicks::Now();
  TimeTicks max_time = TimeTicks::Now() + delay;

  RunLoop();

  // Make sure the sync has happened.
  ASSERT_EQ(1U, r.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(
      Union(types1, types2), r.snapshots[0].source().types));

  // Make sure the sync happened at the right time.
  EXPECT_GE(r.times[0], min_time);
  EXPECT_LE(r.times[0], max_time);
}

// Test nudge scheduling.
TEST_F(SyncSchedulerTest, NudgeWithPayloads) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords records;
  syncable::ModelTypePayloadMap model_types_with_payloads;
  model_types_with_payloads[syncable::BOOKMARKS] = "test";

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))))
      .RetiresOnSaturation();
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_LOCAL, model_types_with_payloads, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_EQ(model_types_with_payloads, records.snapshots[0].source().types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[0].source().updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareRecords records2;
  model_types_with_payloads.erase(syncable::BOOKMARKS);
  model_types_with_payloads[syncable::AUTOFILL] = "test2";
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records2))));
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_LOCAL, model_types_with_payloads, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records2.snapshots.size());
  EXPECT_EQ(model_types_with_payloads, records2.snapshots[0].source().types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records2.snapshots[0].source().updates_source);
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeWithPayloadsCoalescing) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r))));
  syncable::ModelTypePayloadMap types1, types2, types3;
  types1[syncable::BOOKMARKS] = "test1";
  types2[syncable::AUTOFILL] = "test2";
  types3[syncable::THEMES] = "test3";
  TimeDelta delay = zero();
  TimeTicks optimal_time = TimeTicks::Now() + delay;
  scheduler()->ScheduleNudgeWithPayloads(
      delay, NUDGE_SOURCE_UNKNOWN, types1, FROM_HERE);
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_LOCAL, types2, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r.snapshots.size());
  EXPECT_GE(r.times[0], optimal_time);
  syncable::ModelTypePayloadMap coalesced_types;
  syncable::CoalescePayloads(&coalesced_types, types1);
  syncable::CoalescePayloads(&coalesced_types, types2);
  EXPECT_EQ(coalesced_types, r.snapshots[0].source().types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[0].source().updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareRecords r2;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r2))));
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_NOTIFICATION, types3, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r2.snapshots.size());
  EXPECT_EQ(types3, r2.snapshots[0].source().types);
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r2.snapshots[0].source().updates_source);
}

// Test that polling works as expected.
TEST_F(SyncSchedulerTest, Polling) {
  SyncShareRecords records;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShareMultiple(&records, kMinNumSamples))));

  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll_interval);
}

// Test that the short poll interval is used.
TEST_F(SyncSchedulerTest, PollNotificationsDisabled) {
  SyncShareRecords records;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShareMultiple(&records, kMinNumSamples))));

  scheduler()->OnReceivedShortPollIntervalUpdate(poll_interval);
  scheduler()->set_notifications_enabled(false);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling intervals are updated when needed.
TEST_F(SyncSchedulerTest, PollIntervalUpdate) {
  SyncShareRecords records;
  TimeDelta poll1(TimeDelta::FromMilliseconds(120));
  TimeDelta poll2(TimeDelta::FromMilliseconds(30));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll1);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillOnce(WithArg<0>(
          sessions::test_util::SimulatePollIntervalUpdate(poll2)))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulateSuccess),
                WithArg<0>(
                    RecordSyncShareMultiple(&records, kMinNumSamples))));

  TimeTicks optimal_start = TimeTicks::Now() + poll1 + poll2;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll2);
}

// Test that the sessions commit delay is updated when needed.
TEST_F(SyncSchedulerTest, SessionsCommitDelay) {
  SyncShareRecords records;
  TimeDelta delay1(TimeDelta::FromMilliseconds(120));
  TimeDelta delay2(TimeDelta::FromMilliseconds(30));
  scheduler()->OnReceivedSessionsCommitDelay(delay1);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(
          DoAll(
              WithArg<0>(
                  sessions::test_util::SimulateSessionsCommitDelayUpdate(
                      delay2)),
              Invoke(sessions::test_util::SimulateSuccess),
              QuitLoopNowAction()));

  EXPECT_EQ(delay1, scheduler()->sessions_commit_delay());
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  EXPECT_EQ(delay1, scheduler()->sessions_commit_delay());
  const ModelTypeSet model_types(syncable::BOOKMARKS);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  EXPECT_EQ(delay2, scheduler()->sessions_commit_delay());
  StopSyncScheduler();
}

// Test that a sync session is run through to completion.
TEST_F(SyncSchedulerTest, HasMoreToSync) {
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateHasMoreToSync))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      QuitLoopNowAction()));
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeSet(), FROM_HERE);
  RunLoop();
  // If more nudges are scheduled, they'll be waited on by TearDown, and would
  // cause our expectation to break.
}

// Test that no syncing occurs when throttled.
TEST_F(SyncSchedulerTest, ThrottlingDoesThrottle) {
  const ModelTypeSet types(syncable::BOOKMARKS);
  TimeDelta poll(TimeDelta::FromMilliseconds(5));
  TimeDelta throttle(TimeDelta::FromMinutes(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(WithArg<0>(sessions::test_util::SimulateThrottled(throttle)))
      .WillRepeatedly(AddFailureAndQuitLoopNow());

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  PumpLoop();

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      types, GetUpdatesCallerInfo::RECONFIGURATION);
  PumpLoop();
}

TEST_F(SyncSchedulerTest, ThrottlingExpires) {
  SyncShareRecords records;
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(WithArg<0>(sessions::test_util::SimulateThrottled(throttle1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShareMultiple(&records, kMinNumSamples))));

  TimeTicks optimal_start = TimeTicks::Now() + poll + throttle1;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll);
}

// Test nudges / polls don't run in config mode and config tasks do.
TEST_F(SyncSchedulerTest, ConfigurationMode) {
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  SyncShareRecords records;
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce((Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  const ModelTypeSet nudge_types(syncable::AUTOFILL);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, nudge_types, FROM_HERE);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, nudge_types, FROM_HERE);

  const ModelTypeSet config_types(syncable::BOOKMARKS);

  scheduler()->ScheduleConfig(
      config_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeSetToModelTypePayloadMap(config_types,
      records.snapshots[0].source().types));
}

// Have the sycner fail during commit.  Expect that the scheduler enters
// backoff.
TEST_F(BackoffTriggersSyncSchedulerTest, FailCommitOnce) {
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      QuitLoopNowAction()));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail during download updates and succeed on the first
// retry.  Expect that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerTest, FailDownloadOnceThenSucceed) {
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      QuitLoopNowAction()));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail during commit and succeed on the first retry.  Expect
// that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerTest, FailCommitOnceThenSucceed) {
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      QuitLoopNowAction()));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail to download updates and fail again on the retry.
// Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerTest, FailDownloadTwice) {
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillRepeatedly(DoAll(
              Invoke(sessions::test_util::SimulateDownloadUpdatesFailed),
              QuitLoopNowAction()));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Test that no polls or extraneous nudges occur when in backoff.
TEST_F(SyncSchedulerTest, BackoffDropsJobs) {
  SyncShareRecords r;
  TimeDelta poll(TimeDelta::FromMilliseconds(5));
  const ModelTypeSet types(syncable::BOOKMARKS);
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(1)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareMultiple(&r, 1U)));
  EXPECT_CALL(*delay(), GetDelay(_)).
      WillRepeatedly(Return(TimeDelta::FromDays(1)));

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // This nudge should fail and put us into backoff.  Thanks to our mock
  // GetDelay() setup above, this will be a long backoff.
  scheduler()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(1U, r.snapshots.size());
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[0].source().updates_source);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(1)
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShare(&r)));

  // We schedule a nudge with enough delay (10X poll interval) that at least
  // one or two polls would have taken place.  The nudge should succeed.
  scheduler()->ScheduleNudge(poll * 10, NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(delay());
  ASSERT_EQ(2U, r.snapshots.size());
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[1].source().updates_source);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(0);
  EXPECT_CALL(*delay(), GetDelay(_)).Times(0);

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      types, GetUpdatesCallerInfo::RECONFIGURATION);
  PumpLoop();

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  PumpLoop();
}

// Test that backoff is shaping traffic properly with consecutive errors.
TEST_F(SyncSchedulerTest, BackoffElevation) {
  SyncShareRecords r;
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(kMinNumSamples)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareMultiple(&r, kMinNumSamples)));

  const TimeDelta first = TimeDelta::FromSeconds(1);
  const TimeDelta second = TimeDelta::FromMilliseconds(2);
  const TimeDelta third = TimeDelta::FromMilliseconds(3);
  const TimeDelta fourth = TimeDelta::FromMilliseconds(4);
  const TimeDelta fifth = TimeDelta::FromMilliseconds(5);
  const TimeDelta sixth = TimeDelta::FromDays(1);

  EXPECT_CALL(*delay(), GetDelay(Eq(first))).WillOnce(Return(second))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(second))).WillOnce(Return(third))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(third))).WillOnce(Return(fourth))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(fourth))).WillOnce(Return(fifth))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(fifth))).WillOnce(Return(sixth));

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again with a nudge.
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeSet(), FROM_HERE);
  RunLoop();

  ASSERT_EQ(kMinNumSamples, r.snapshots.size());
  EXPECT_GE(r.times[1] - r.times[0], second);
  EXPECT_GE(r.times[2] - r.times[1], third);
  EXPECT_GE(r.times[3] - r.times[2], fourth);
  EXPECT_GE(r.times[4] - r.times[3], fifth);
}

// Test that things go back to normal once a retry makes forward progress.
TEST_F(SyncSchedulerTest, BackoffRelief) {
  SyncShareRecords r;
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  const TimeDelta backoff = TimeDelta::FromMilliseconds(5);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      RecordSyncShareMultiple(&r, kMinNumSamples)))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                            RecordSyncShareMultiple(&r, kMinNumSamples)));
  EXPECT_CALL(*delay(), GetDelay(_)).WillOnce(Return(backoff));

  // Optimal start for the post-backoff poll party.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  scheduler()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL,
                             ModelTypeSet(), FROM_HERE);
  RunLoop();

  StopSyncScheduler();

  EXPECT_EQ(kMinNumSamples, r.times.size());

  // The first nudge ran as soon as possible.  It failed.
  TimeTicks optimal_job_time = optimal_start;
  EXPECT_GE(r.times[0], optimal_job_time);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[0].source().updates_source);

  // It was followed by a successful retry nudge shortly afterward.
  optimal_job_time = optimal_job_time + backoff;
  EXPECT_GE(r.times[1], optimal_job_time);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[1].source().updates_source);
  // After that, we went back to polling.
  for (size_t i = 2; i < r.snapshots.size(); i++) {
    optimal_job_time = optimal_job_time + poll;
    SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
    EXPECT_GE(r.times[i], optimal_job_time);
    EXPECT_EQ(GetUpdatesCallerInfo::PERIODIC,
              r.snapshots[i].source().updates_source);
  }
}

// Test that poll failures are ignored.  They should have no effect on
// subsequent poll attempts, nor should they trigger a backoff/retry.
TEST_F(SyncSchedulerTest, TransientPollFailure) {
  SyncShareRecords r;
  const TimeDelta poll_interval(TimeDelta::FromMilliseconds(1));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);
  UseMockDelayProvider(); // Will cause test failure if backoff is initiated.

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      RecordSyncShare(&r)))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      RecordSyncShare(&r)));

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run the unsucessful poll.  The failed poll should not trigger backoff.
  RunLoop();
  EXPECT_FALSE(scheduler()->IsBackingOff());

  // Run the successful poll.
  RunLoop();
  EXPECT_FALSE(scheduler()->IsBackingOff());
}

TEST_F(SyncSchedulerTest, GetRecommendedDelay) {
  EXPECT_LE(TimeDelta::FromSeconds(0),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(0)));
  EXPECT_LE(TimeDelta::FromSeconds(1),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(1)));
  EXPECT_LE(TimeDelta::FromSeconds(50),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(50)));
  EXPECT_LE(TimeDelta::FromSeconds(10),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(10)));
  EXPECT_EQ(TimeDelta::FromSeconds(kMaxBackoffSeconds),
            SyncScheduler::GetRecommendedDelay(
                TimeDelta::FromSeconds(kMaxBackoffSeconds)));
  EXPECT_EQ(TimeDelta::FromSeconds(kMaxBackoffSeconds),
            SyncScheduler::GetRecommendedDelay(
                TimeDelta::FromSeconds(kMaxBackoffSeconds + 1)));
}

// Test that appropriate syncer steps are requested for each job type.
TEST_F(SyncSchedulerTest, SyncerSteps) {
  // Nudges.
  EXPECT_CALL(*syncer(), SyncShare(_, SYNCER_BEGIN, SYNCER_END))
      .Times(1);
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeSet(), FROM_HERE);
  PumpLoop();
  // Pump again to run job.
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // ClearUserData.
  EXPECT_CALL(*syncer(), SyncShare(_, CLEAR_PRIVATE_DATA, CLEAR_PRIVATE_DATA))
      .Times(1);
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleClearUserData();
  PumpLoop();
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // Configuration.
  EXPECT_CALL(*syncer(), SyncShare(_, DOWNLOAD_UPDATES, APPLY_UPDATES));
  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      ModelTypeSet(), GetUpdatesCallerInfo::RECONFIGURATION);
  PumpLoop();
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // Cleanup disabled types.
  EXPECT_CALL(*syncer(),
              SyncShare(_, CLEANUP_DISABLED_TYPES, CLEANUP_DISABLED_TYPES));
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleCleanupDisabledTypes();
  // Only need to pump once, as ScheduleCleanupDisabledTypes()
  // schedules the job directly.
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // Poll.
  EXPECT_CALL(*syncer(), SyncShare(_, SYNCER_BEGIN, SYNCER_END))
      .Times(AtLeast(1))
      .WillRepeatedly(QuitLoopNowAction());
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());
}

// Test config tasks don't run during normal mode.
// TODO(tim): Implement this test and then the functionality!
TEST_F(SyncSchedulerTest, DISABLED_NoConfigDuringNormal) {
}

// Test that starting the syncer thread without a valid connection doesn't
// break things when a connection is detected.
TEST_F(SyncSchedulerTest, StartWhenNotConnected) {
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
    .WillOnce(Invoke(sessions::test_util::SimulateConnectionFailure))
    .WillOnce(QuitLoopNowAction());
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  MessageLoop::current()->RunAllPending();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeSet(), FROM_HERE);
  // Should save the nudge for until after the server is reachable.
  MessageLoop::current()->RunAllPending();

  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  scheduler()->OnConnectionStatusChange();
  MessageLoop::current()->RunAllPending();
}

TEST_F(SyncSchedulerTest, SetsPreviousRoutingInfo) {
  ModelSafeRoutingInfo info;
  EXPECT_TRUE(info == context()->previous_session_routing_info());
  ModelSafeRoutingInfo expected;
  context()->registrar()->GetModelSafeRoutingInfo(&expected);
  ASSERT_FALSE(expected.empty());
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(1);

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeSet(), FROM_HERE);
  PumpLoop();
  // Pump again to run job.
  PumpLoop();

  StopSyncScheduler();

  EXPECT_TRUE(expected == context()->previous_session_routing_info());
}

}  // namespace browser_sync
