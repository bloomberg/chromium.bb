// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "sync/engine/backoff_delay_provider.h"
#include "sync/engine/sync_scheduler_impl.h"
#include "sync/engine/syncer.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/sessions/test_util.h"
#include "sync/test/callback_counter.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/mock_connection_manager.h"
#include "sync/test/engine/mock_nudge_handler.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/mock_invalidation.h"
#include "sync/util/extensions_activity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;
using testing::WithArgs;
using testing::WithoutArgs;

namespace syncer {
using sessions::SyncSession;
using sessions::SyncSessionContext;
using sync_pb::GetUpdatesCallerInfo;

class MockSyncer : public Syncer {
 public:
  MockSyncer();
  MOCK_METHOD3(NormalSyncShare,
               bool(ModelTypeSet,
                    sessions::NudgeTracker*,
                    sessions::SyncSession*));
  MOCK_METHOD3(ConfigureSyncShare,
               bool(ModelTypeSet,
                    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource,
                    SyncSession*));
  MOCK_METHOD2(PollSyncShare, bool(ModelTypeSet, sessions::SyncSession*));
};

MockSyncer::MockSyncer()
  : Syncer(NULL) {}

typedef std::vector<TimeTicks> SyncShareTimes;

void QuitLoopNow() {
  // We use QuitNow() instead of Quit() as the latter may get stalled
  // indefinitely in the presence of repeated timers with low delays
  // and a slow test (e.g., ThrottlingDoesThrottle [which has a poll
  // delay of 5ms] run under TSAN on the trybots).
  base::MessageLoop::current()->QuitNow();
}

void RunLoop() {
  base::MessageLoop::current()->Run();
}

void PumpLoop() {
  // Do it this way instead of RunAllPending to pump loop exactly once
  // (necessary in the presence of timers; see comment in
  // QuitLoopNow).
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&QuitLoopNow));
  RunLoop();
}

void PumpLoopFor(base::TimeDelta time) {
  // Allow the loop to run for the specified amount of time.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&QuitLoopNow), time);
  RunLoop();
}

ModelSafeRoutingInfo TypesToRoutingInfo(ModelTypeSet types) {
  ModelSafeRoutingInfo routes;
  for (ModelTypeSet::Iterator iter = types.First(); iter.Good(); iter.Inc()) {
    routes[iter.Get()] = GROUP_PASSIVE;
  }
  return routes;
}


static const size_t kMinNumSamples = 5;

// Test harness for the SyncScheduler.  Test the delays and backoff timers used
// in response to various events.
//
// These tests execute in real time with real timers.  We try to keep the
// delays short, but there is a limit to how short we can make them.  The
// timers on some platforms (ie. Windows) have a timer resolution greater than
// 1ms.  Using 1ms delays may result in test flakiness.
//
// See crbug.com/402212 for more info.
class SyncSchedulerTest : public testing::Test {
 public:
  SyncSchedulerTest() : syncer_(NULL), delay_(NULL), weak_ptr_factory_(this) {}

  class MockDelayProvider : public BackoffDelayProvider {
   public:
    MockDelayProvider() : BackoffDelayProvider(
        TimeDelta::FromSeconds(kInitialBackoffRetrySeconds),
        TimeDelta::FromSeconds(kInitialBackoffImmediateRetrySeconds)) {
    }

    MOCK_METHOD1(GetDelay, TimeDelta(const TimeDelta&));
  };

  void SetUp() override {
    dir_maker_.SetUp();
    syncer_ = new testing::StrictMock<MockSyncer>();
    delay_ = NULL;
    extensions_activity_ = new ExtensionsActivity();

    routing_info_[THEMES] = GROUP_UI;
    routing_info_[TYPED_URLS] = GROUP_DB;
    routing_info_[THEMES] = GROUP_UI;
    routing_info_[NIGORI] = GROUP_PASSIVE;

    workers_.clear();
    workers_.push_back(make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers_.push_back(make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    workers_.push_back(make_scoped_refptr(new FakeModelWorker(GROUP_PASSIVE)));

    connection_.reset(new MockConnectionManager(directory(),
                                                &cancelation_signal_));
    connection_->SetServerReachable();

    model_type_registry_.reset(
        new ModelTypeRegistry(workers_, directory(), &mock_nudge_handler_));

    context_.reset(new SyncSessionContext(
            connection_.get(), directory(),
            extensions_activity_.get(),
            std::vector<SyncEngineEventListener*>(), NULL,
            model_type_registry_.get(),
            true,  // enable keystore encryption
            false,  // force enable pre-commit GU avoidance
            "fake_invalidator_client_id"));
    context_->SetRoutingInfo(routing_info_);
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    scheduler_.reset(
        new SyncSchedulerImpl("TestSyncScheduler",
            BackoffDelayProvider::FromDefaults(),
            context(),
            syncer_));
    scheduler_->SetDefaultNudgeDelay(default_delay());
  }

  SyncSchedulerImpl* scheduler() { return scheduler_.get(); }
  const ModelSafeRoutingInfo& routing_info() { return routing_info_; }
  MockSyncer* syncer() { return syncer_; }
  MockDelayProvider* delay() { return delay_; }
  MockConnectionManager* connection() { return connection_.get(); }
  TimeDelta default_delay() { return TimeDelta::FromSeconds(0); }
  TimeDelta timeout() {
    return TestTimeouts::action_timeout();
  }

  void TearDown() override {
    PumpLoop();
    scheduler_.reset();
    PumpLoop();
    dir_maker_.TearDown();
  }

  void AnalyzePollRun(const SyncShareTimes& times, size_t min_num_samples,
      const TimeTicks& optimal_start, const TimeDelta& poll_interval) {
    EXPECT_GE(times.size(), min_num_samples);
    for (size_t i = 0; i < times.size(); i++) {
      SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
      TimeTicks optimal_next_sync = optimal_start + poll_interval * i;
      EXPECT_GE(times[i], optimal_next_sync);
    }
  }

  void DoQuitLoopNow() {
    QuitLoopNow();
  }

  void StartSyncConfiguration() {
    scheduler()->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  }

  void StartSyncScheduler(base::Time last_poll_time) {
    scheduler()->Start(SyncScheduler::NORMAL_MODE, last_poll_time);
  }

  // This stops the scheduler synchronously.
  void StopSyncScheduler() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSchedulerTest::DoQuitLoopNow,
                   weak_ptr_factory_.GetWeakPtr()));
    RunLoop();
  }

  bool RunAndGetBackoff() {
    ModelTypeSet nudge_types(THEMES);
    StartSyncScheduler(base::Time());

    scheduler()->ScheduleLocalNudge(nudge_types, FROM_HERE);
    RunLoop();

    return scheduler()->IsBackingOff();
  }

  void UseMockDelayProvider() {
    delay_ = new MockDelayProvider();
    scheduler_->delay_provider_.reset(delay_);
  }

  SyncSessionContext* context() { return context_.get(); }

  ModelTypeSet GetThrottledTypes() {
    return scheduler_->nudge_tracker_.GetThrottledTypes();
  }

  base::TimeDelta GetRetryTimerDelay() {
    EXPECT_TRUE(scheduler_->retry_timer_.IsRunning());
    return scheduler_->retry_timer_.GetCurrentDelay();
  }

  static scoped_ptr<InvalidationInterface> BuildInvalidation(
      int64 version,
      const std::string& payload) {
    return MockInvalidation::Build(version, payload);
  }

 private:
  syncable::Directory* directory() {
    return dir_maker_.directory();
  }

  base::MessageLoop loop_;
  TestDirectorySetterUpper dir_maker_;
  CancelationSignal cancelation_signal_;
  scoped_ptr<MockConnectionManager> connection_;
  scoped_ptr<ModelTypeRegistry> model_type_registry_;
  scoped_ptr<SyncSessionContext> context_;
  scoped_ptr<SyncSchedulerImpl> scheduler_;
  MockNudgeHandler mock_nudge_handler_;
  MockSyncer* syncer_;
  MockDelayProvider* delay_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;
  ModelSafeRoutingInfo routing_info_;
  base::WeakPtrFactory<SyncSchedulerTest> weak_ptr_factory_;
};

void RecordSyncShareImpl(SyncShareTimes* times) {
  times->push_back(TimeTicks::Now());
}

ACTION_P2(RecordSyncShare, times, success) {
  RecordSyncShareImpl(times);
  if (base::MessageLoop::current()->is_running())
    QuitLoopNow();
  return success;
}

ACTION_P3(RecordSyncShareMultiple, times, quit_after, success) {
  RecordSyncShareImpl(times);
  EXPECT_LE(times->size(), quit_after);
  if (times->size() >= quit_after &&
      base::MessageLoop::current()->is_running()) {
    QuitLoopNow();
  }
  return success;
}

ACTION_P(StopScheduler, scheduler) {
  scheduler->Stop();
}

ACTION(AddFailureAndQuitLoopNow) {
  ADD_FAILURE();
  QuitLoopNow();
  return true;
}

ACTION_P(QuitLoopNowAction, success) {
  QuitLoopNow();
  return success;
}

// Test nudge scheduling.
TEST_F(SyncSchedulerTest, Nudge) {
  SyncShareTimes times;
  ModelTypeSet model_types(THEMES);

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareTimes times2;
  model_types.Remove(THEMES);
  model_types.Put(TYPED_URLS);
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));
  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);
  RunLoop();
}

// Make sure a regular config command is scheduled fine in the absence of any
// errors.
TEST_F(SyncSchedulerTest, Config) {
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      model_types,
      TypesToRoutingInfo(model_types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(1, ready_counter.times_called());
  ASSERT_EQ(0, retry_counter.times_called());
}

// Simulate a failure and make sure the config request is retried.
TEST_F(SyncSchedulerTest, ConfigWithBackingOff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(20)));
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  StartSyncConfiguration();

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));

  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      model_types,
      TypesToRoutingInfo(model_types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  ASSERT_EQ(1, retry_counter.times_called());

  // RunLoop() will trigger TryCanaryJob which will retry configuration.
  // Since retry_task was already called it shouldn't be called again.
  RunLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  ASSERT_EQ(1, retry_counter.times_called());

  Mock::VerifyAndClearExpectations(syncer());

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();

  ASSERT_EQ(1, ready_counter.times_called());
}

// Simuilate SyncSchedulerImpl::Stop being called in the middle of Configure.
// This can happen if server returns NOT_MY_BIRTHDAY.
TEST_F(SyncSchedulerTest, ConfigWithStop) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(20)));
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  StartSyncConfiguration();

  // Make ConfigureSyncShare call scheduler->Stop(). It is not supposed to call
  // retry_task or dereference configuration params.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureFailed),
                      StopScheduler(scheduler()),
                      RecordSyncShare(&times, false)));

  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      model_types,
      TypesToRoutingInfo(model_types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  ASSERT_EQ(0, retry_counter.times_called());
}

// Issue a nudge when the config has failed. Make sure both the config and
// nudge are executed.
TEST_F(SyncSchedulerTest, NudgeWithConfigWithBackingOff) {
  const ModelTypeSet model_types(THEMES);
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(50)));
  SyncShareTimes times;

  StartSyncConfiguration();

  // Request a configure and make sure it fails.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));
  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      model_types,
      TypesToRoutingInfo(model_types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  ASSERT_EQ(1, retry_counter.times_called());
  Mock::VerifyAndClearExpectations(syncer());

  // Ask for a nudge while dealing with repeated configure failure.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));
  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);
  RunLoop();
  // Note that we're not RunLoop()ing for the NUDGE we just scheduled, but
  // for the first retry attempt from the config job (after
  // waiting ~+/- 50ms).
  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(0, ready_counter.times_called());

  // Let the next configure retry succeed.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();

  // Now change the mode so nudge can execute.
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  StartSyncScheduler(base::Time());
  PumpLoop();
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeCoalescing) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  const ModelTypeSet types1(THEMES), types2(TYPED_URLS), types3(THEMES);
  TimeTicks optimal_time = TimeTicks::Now() + default_delay();
  scheduler()->ScheduleLocalNudge(types1, FROM_HERE);
  scheduler()->ScheduleLocalNudge(types2, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], optimal_time);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));
  scheduler()->ScheduleLocalNudge(types3, FROM_HERE);
  RunLoop();
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeCoalescingWithDifferentTimings) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  ModelTypeSet types1(THEMES), types2(TYPED_URLS), types3;

  // Create a huge time delay.
  TimeDelta delay = TimeDelta::FromDays(1);

  std::map<ModelType, TimeDelta> delay_map;
  delay_map[types1.First().Get()] = delay;
  scheduler()->OnReceivedCustomNudgeDelays(delay_map);
  scheduler()->ScheduleLocalNudge(types1, FROM_HERE);
  scheduler()->ScheduleLocalNudge(types2, FROM_HERE);

  TimeTicks min_time = TimeTicks::Now();
  TimeTicks max_time = TimeTicks::Now() + delay;

  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  // Make sure the sync happened at the right time.
  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], min_time);
  EXPECT_LE(times[0], max_time);
}

// Test nudge scheduling.
TEST_F(SyncSchedulerTest, NudgeWithStates) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times1;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times1, true)))
      .RetiresOnSaturation();
  scheduler()->ScheduleInvalidationNudge(
      THEMES, BuildInvalidation(10, "test"), FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));
  scheduler()->ScheduleInvalidationNudge(
      TYPED_URLS, BuildInvalidation(10, "test2"), FROM_HERE);
  RunLoop();
}

// Test that polling works as expected.
TEST_F(SyncSchedulerTest, Polling) {
  SyncShareTimes times;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), PollSyncShare(_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that we reuse the previous poll time on startup, triggering the first
// poll based on when the last one happened. Subsequent polls should have the
// normal delay.
TEST_F(SyncSchedulerTest, PollingPersistence) {
  SyncShareTimes times;
  // Use a large poll interval that wouldn't normally get hit on its own for
  // some time yet.
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(500));
  EXPECT_CALL(*syncer(), PollSyncShare(_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);

  // Set the start time to now, as the poll was overdue.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(base::Time::Now() - poll_interval);

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that if the persisted poll is in the future, it's ignored (the case
// where the local time has been modified).
TEST_F(SyncSchedulerTest, PollingPersistenceBadClock) {
  SyncShareTimes times;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), PollSyncShare(_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);

  // Set the start time to |poll_interval| in the future.
  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time::Now() + base::TimeDelta::FromMinutes(10));

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that the short poll interval is used.
TEST_F(SyncSchedulerTest, PollNotificationsDisabled) {
  SyncShareTimes times;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), PollSyncShare(_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedShortPollIntervalUpdate(poll_interval);
  scheduler()->SetNotificationsEnabled(false);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling intervals are updated when needed.
TEST_F(SyncSchedulerTest, PollIntervalUpdate) {
  SyncShareTimes times;
  TimeDelta poll1(TimeDelta::FromMilliseconds(120));
  TimeDelta poll2(TimeDelta::FromMilliseconds(30));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll1);
  EXPECT_CALL(*syncer(), PollSyncShare(_,_)).Times(AtLeast(kMinNumSamples))
      .WillOnce(DoAll(
          WithArgs<0,1>(
              sessions::test_util::SimulatePollIntervalUpdate(poll2)),
          Return(true)))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                WithArg<1>(
                    RecordSyncShareMultiple(&times, kMinNumSamples, true))));

  TimeTicks optimal_start = TimeTicks::Now() + poll1 + poll2;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll2);
}

// Test that no syncing occurs when throttled.
TEST_F(SyncSchedulerTest, ThrottlingDoesThrottle) {
  const ModelTypeSet types(THEMES);
  TimeDelta poll(TimeDelta::FromMilliseconds(20));
  TimeDelta throttle(TimeDelta::FromMinutes(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(
          WithArg<2>(sessions::test_util::SimulateThrottled(throttle)),
          Return(false)))
      .WillRepeatedly(AddFailureAndQuitLoopNow());

  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      types,
      TypesToRoutingInfo(types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  ASSERT_EQ(1, retry_counter.times_called());

}

TEST_F(SyncSchedulerTest, ThrottlingExpiresFromPoll) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), PollSyncShare(_,_))
      .WillOnce(DoAll(
          WithArg<1>(sessions::test_util::SimulateThrottled(throttle1)),
          Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), PollSyncShare(_,_))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  TimeTicks optimal_start = TimeTicks::Now() + poll + throttle1;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll);
}

TEST_F(SyncSchedulerTest, ThrottlingExpiresFromNudge) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          WithArg<2>(sessions::test_util::SimulateThrottled(throttle1)),
          Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));

  const ModelTypeSet types(THEMES);
  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);

  PumpLoop(); // To get PerformDelayedNudge called.
  PumpLoop(); // To get TrySyncSessionJob called
  EXPECT_TRUE(scheduler()->IsCurrentlyThrottled());
  RunLoop();
  EXPECT_FALSE(scheduler()->IsCurrentlyThrottled());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerTest, ThrottlingExpiresFromConfigure) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(
          WithArg<2>(sessions::test_util::SimulateThrottled(throttle1)),
          Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureSuccess),
                      QuitLoopNowAction(true)));

  const ModelTypeSet types(THEMES);
  StartSyncConfiguration();

  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      types,
      TypesToRoutingInfo(types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  EXPECT_EQ(0, ready_counter.times_called());
  EXPECT_EQ(1, retry_counter.times_called());
  EXPECT_TRUE(scheduler()->IsCurrentlyThrottled());

  RunLoop();
  EXPECT_FALSE(scheduler()->IsCurrentlyThrottled());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerTest, TypeThrottlingBlocksNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(default_delay()));

  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromSeconds(60));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          WithArg<2>(
              sessions::test_util::SimulateTypesThrottled(types, throttle1)),
          Return(false)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop(); // To get PerformDelayedNudge called.
  PumpLoop(); // To get TrySyncSessionJob called
  EXPECT_TRUE(GetThrottledTypes().HasAll(types));

  // This won't cause a sync cycle because the types are throttled.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerTest, TypeThrottlingDoesBlockOtherSources) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(default_delay()));

  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromSeconds(60));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  const ModelTypeSet throttled_types(THEMES);
  const ModelTypeSet unthrottled_types(PREFERENCES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          WithArg<2>(
              sessions::test_util::SimulateTypesThrottled(
                  throttled_types, throttle1)),
          Return(false)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(throttled_types, FROM_HERE);
  PumpLoop(); // To get PerformDelayedNudge called.
  PumpLoop(); // To get TrySyncSessionJob called
  EXPECT_TRUE(GetThrottledTypes().HasAll(throttled_types));

  // Ignore invalidations for throttled types.
  scheduler()->ScheduleInvalidationNudge(
      THEMES, BuildInvalidation(10, "test"), FROM_HERE);
  PumpLoop();

  // Ignore refresh requests for throttled types.
  scheduler()->ScheduleLocalRefreshRequest(throttled_types, FROM_HERE);
  PumpLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Local nudges for non-throttled types will trigger a sync.
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                            RecordSyncShare(&times, true)));
  scheduler()->ScheduleLocalNudge(unthrottled_types, FROM_HERE);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  StopSyncScheduler();
}

// Test nudges / polls don't run in config mode and config tasks do.
TEST_F(SyncSchedulerTest, ConfigurationMode) {
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  SyncShareTimes times;
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  StartSyncConfiguration();

  const ModelTypeSet nudge_types(TYPED_URLS);
  scheduler()->ScheduleLocalNudge(nudge_types, FROM_HERE);
  scheduler()->ScheduleLocalNudge(nudge_types, FROM_HERE);

  const ModelTypeSet config_types(THEMES);

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)))
      .RetiresOnSaturation();
  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      config_types,
      TypesToRoutingInfo(config_types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();
  ASSERT_EQ(1, ready_counter.times_called());
  ASSERT_EQ(0, retry_counter.times_called());

  Mock::VerifyAndClearExpectations(syncer());

  // Switch to NORMAL_MODE to ensure NUDGES were properly saved and run.
  scheduler()->OnReceivedLongPollIntervalUpdate(TimeDelta::FromDays(1));
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));

  // TODO(tim): Figure out how to remove this dangerous need to reset
  // routing info between mode switches.
  context()->SetRoutingInfo(routing_info());
  StartSyncScheduler(base::Time());

  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
}

class BackoffTriggersSyncSchedulerTest : public SyncSchedulerTest {
  void SetUp() override {
    SyncSchedulerTest::SetUp();
    UseMockDelayProvider();
    EXPECT_CALL(*delay(), GetDelay(_))
        .WillRepeatedly(Return(TimeDelta::FromMilliseconds(10)));
  }

  void TearDown() override {
    StopSyncScheduler();
    SyncSchedulerTest::TearDown();
  }
};

// Have the syncer fail during commit.  Expect that the scheduler enters
// backoff.
TEST_F(BackoffTriggersSyncSchedulerTest, FailCommitOnce) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      QuitLoopNowAction(false)));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail during download updates and succeed on the first
// retry.  Expect that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerTest, FailDownloadOnceThenSucceed) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          Invoke(sessions::test_util::SimulateDownloadUpdatesFailed),
          Return(false)))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail during commit and succeed on the first retry.  Expect
// that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerTest, FailCommitOnceThenSucceed) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          Invoke(sessions::test_util::SimulateCommitFailed),
          Return(false)))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail to download updates and fail again on the retry.
// Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerTest, FailDownloadTwice) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          Invoke(sessions::test_util::SimulateDownloadUpdatesFailed),
          Return(false)))
      .WillRepeatedly(DoAll(
          Invoke(sessions::test_util::SimulateDownloadUpdatesFailed),
          QuitLoopNowAction(false)));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail to get the encryption key yet succeed in downloading
// updates. Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerTest, FailGetEncryptionKey) {
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillOnce(DoAll(
          Invoke(sessions::test_util::SimulateGetEncryptionKeyFailed),
          Return(false)))
      .WillRepeatedly(DoAll(
          Invoke(sessions::test_util::SimulateGetEncryptionKeyFailed),
          QuitLoopNowAction(false)));
  StartSyncConfiguration();

  ModelTypeSet types(THEMES);
  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      types,
      TypesToRoutingInfo(types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();

  EXPECT_TRUE(scheduler()->IsBackingOff());
}

// Test that no polls or extraneous nudges occur when in backoff.
TEST_F(SyncSchedulerTest, BackoffDropsJobs) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromMilliseconds(10));
  const ModelTypeSet types(THEMES);
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      RecordSyncShareMultiple(&times, 1U, false)));
  EXPECT_CALL(*delay(), GetDelay(_)).
      WillRepeatedly(Return(TimeDelta::FromDays(1)));

  StartSyncScheduler(base::Time());

  // This nudge should fail and put us into backoff.  Thanks to our mock
  // GetDelay() setup above, this will be a long backoff.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  RunLoop();

  // From this point forward, no SyncShare functions should be invoked.
  Mock::VerifyAndClearExpectations(syncer());

  // Wait a while (10x poll interval) so a few poll jobs will be attempted.
  PumpLoopFor(poll * 10);

  // Try (and fail) to schedule a nudge.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);

  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(delay());

  EXPECT_CALL(*delay(), GetDelay(_)).Times(0);

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      types,
      TypesToRoutingInfo(types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  ASSERT_EQ(1, retry_counter.times_called());

}

// Test that backoff is shaping traffic properly with consecutive errors.
TEST_F(SyncSchedulerTest, BackoffElevation) {
  SyncShareTimes times;
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_)).Times(kMinNumSamples)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareMultiple(&times, kMinNumSamples, false)));

  const TimeDelta first = TimeDelta::FromSeconds(kInitialBackoffRetrySeconds);
  const TimeDelta second = TimeDelta::FromMilliseconds(20);
  const TimeDelta third = TimeDelta::FromMilliseconds(30);
  const TimeDelta fourth = TimeDelta::FromMilliseconds(40);
  const TimeDelta fifth = TimeDelta::FromMilliseconds(50);
  const TimeDelta sixth = TimeDelta::FromDays(1);

  EXPECT_CALL(*delay(), GetDelay(first)).WillOnce(Return(second))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(second)).WillOnce(Return(third))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(third)).WillOnce(Return(fourth))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(fourth)).WillOnce(Return(fifth))
          .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(fifth)).WillOnce(Return(sixth));

  StartSyncScheduler(base::Time());

  // Run again with a nudge.
  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  RunLoop();

  ASSERT_EQ(kMinNumSamples, times.size());
  EXPECT_GE(times[1] - times[0], second);
  EXPECT_GE(times[2] - times[1], third);
  EXPECT_GE(times[3] - times[2], fourth);
  EXPECT_GE(times[4] - times[3], fifth);
}

// Test that things go back to normal once a retry makes forward progress.
TEST_F(SyncSchedulerTest, BackoffRelief) {
  SyncShareTimes times;
  UseMockDelayProvider();

  const TimeDelta backoff = TimeDelta::FromMilliseconds(10);
  EXPECT_CALL(*delay(), GetDelay(_)).WillOnce(Return(backoff));

  // Optimal start for the post-backoff poll party.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(base::Time());

  // Kick off the test with a failed nudge.
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      RecordSyncShare(&times, false)));
  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  TimeTicks optimal_job_time = optimal_start;
  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], optimal_job_time);

  // The retry succeeds.
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
              Invoke(sessions::test_util::SimulateNormalSuccess),
              RecordSyncShare(&times, true)));
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  optimal_job_time = optimal_job_time + backoff;
  ASSERT_EQ(2U, times.size());
  EXPECT_GE(times[1], optimal_job_time);

  // Now let the Poll timer do its thing.
  EXPECT_CALL(*syncer(), PollSyncShare(_,_))
      .WillRepeatedly(DoAll(
              Invoke(sessions::test_util::SimulatePollSuccess),
              RecordSyncShareMultiple(&times, kMinNumSamples, true)));
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  // The new optimal time is now, since the desired poll should have happened
  // in the past.
  optimal_job_time = base::TimeTicks::Now();
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(kMinNumSamples, times.size());
  for (size_t i = 2; i < times.size(); i++) {
    SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
    EXPECT_GE(times[i], optimal_job_time);
    optimal_job_time = optimal_job_time + poll;
  }

  StopSyncScheduler();
}

// Test that poll failures are treated like any other failure. They should
// result in retry with backoff.
TEST_F(SyncSchedulerTest, TransientPollFailure) {
  SyncShareTimes times;
  const TimeDelta poll_interval(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);
  UseMockDelayProvider(); // Will cause test failure if backoff is initiated.
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(0)));

  EXPECT_CALL(*syncer(), PollSyncShare(_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulatePollFailed),
                      RecordSyncShare(&times, false)))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncScheduler(base::Time());

  // Run the unsucessful poll. The failed poll should not trigger backoff.
  RunLoop();
  EXPECT_TRUE(scheduler()->IsBackingOff());

  // Run the successful poll.
  RunLoop();
  EXPECT_FALSE(scheduler()->IsBackingOff());
}

// Test that starting the syncer thread without a valid connection doesn't
// break things when a connection is detected.
TEST_F(SyncSchedulerTest, StartWhenNotConnected) {
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConnectionFailure),
                    Return(false)))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                    Return(true)));
  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  // Should save the nudge for until after the server is reachable.
  base::RunLoop().RunUntilIdle();

  scheduler()->OnConnectionStatusChange();
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSchedulerTest, ServerConnectionChangeDuringBackoff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(0)));

  StartSyncScheduler(base::Time());
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConnectionFailure),
                    Return(false)))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                    Return(true)));

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  PumpLoop(); // To get PerformDelayedNudge called.
  PumpLoop(); // Run the nudge, that will fail and schedule a quick retry.
  ASSERT_TRUE(scheduler()->IsBackingOff());

  // Before we run the scheduled canary, trigger a server connection change.
  scheduler()->OnConnectionStatusChange();
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  base::RunLoop().RunUntilIdle();
}

// This was supposed to test the scenario where we receive a nudge while a
// connection change canary is scheduled, but has not run yet.  Since we've made
// the connection change canary synchronous, this is no longer possible.
TEST_F(SyncSchedulerTest, ConnectionChangeCanaryPreemptedByNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(0)));

  StartSyncScheduler(base::Time());
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateConnectionFailure),
                    Return(false)))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                    Return(true)))
    .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                    QuitLoopNowAction(true)));

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);

  PumpLoop(); // To get PerformDelayedNudge called.
  PumpLoop(); // Run the nudge, that will fail and schedule a quick retry.
  ASSERT_TRUE(scheduler()->IsBackingOff());

  // Before we run the scheduled canary, trigger a server connection change.
  scheduler()->OnConnectionStatusChange();
  PumpLoop();
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// Tests that we don't crash trying to run two canaries at once if we receive
// extra connection status change notifications.  See crbug.com/190085.
TEST_F(SyncSchedulerTest, DoubleCanaryInConfigure) {
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_,_,_))
      .WillRepeatedly(DoAll(
              Invoke(sessions::test_util::SimulateConfigureConnectionFailure),
              Return(true)));
  StartSyncConfiguration();
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  ModelTypeSet model_types(THEMES);
  CallbackCounter ready_counter;
  CallbackCounter retry_counter;
  ConfigurationParams params(
      GetUpdatesCallerInfo::RECONFIGURATION,
      model_types,
      TypesToRoutingInfo(model_types),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)),
      base::Bind(&CallbackCounter::Callback, base::Unretained(&retry_counter)));
  scheduler()->ScheduleConfiguration(params);

  scheduler()->OnConnectionStatusChange();
  scheduler()->OnConnectionStatusChange();

  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
}

TEST_F(SyncSchedulerTest, PollFromCanaryAfterAuthError) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), PollSyncShare(_,_))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  connection()->SetServerStatus(HttpResponse::SYNC_AUTH_ERROR);
  StartSyncScheduler(base::Time());

  // Run to wait for polling.
  RunLoop();

  // Normally OnCredentialsUpdated calls TryCanaryJob that doesn't run Poll,
  // but after poll finished with auth error from poll timer it should retry
  // poll once more
  EXPECT_CALL(*syncer(), PollSyncShare(_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulatePollSuccess),
                      RecordSyncShare(&times, true)));
  scheduler()->OnCredentialsUpdated();
  connection()->SetServerStatus(HttpResponse::SERVER_CONNECTION_OK);
  RunLoop();
  StopSyncScheduler();
}

TEST_F(SyncSchedulerTest, SuccessfulRetry) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  scheduler()->OnReceivedGuRetryDelay(delay);
  EXPECT_EQ(delay, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(
          DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                RecordSyncShare(&times, true)));

  // Run to wait for retrying.
  RunLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerTest, FailedRetry) {
  SyncShareTimes times;

  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(10)));

  StartSyncScheduler(base::Time());

  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  scheduler()->OnReceivedGuRetryDelay(delay);

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(
          DoAll(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed),
                RecordSyncShare(&times, false)));

  // Run to wait for retrying.
  RunLoop();

  EXPECT_TRUE(scheduler()->IsBackingOff());
  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(
          DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                RecordSyncShare(&times, true)));

  // Run to wait for second retrying.
  RunLoop();

  StopSyncScheduler();
}

ACTION_P2(VerifyRetryTimerDelay, scheduler_test, expected_delay) {
  EXPECT_EQ(expected_delay, scheduler_test->GetRetryTimerDelay());
}

TEST_F(SyncSchedulerTest, ReceiveNewRetryDelay) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(100);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(200);

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  scheduler()->OnReceivedGuRetryDelay(delay1);
  EXPECT_EQ(delay1, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(
          WithoutArgs(VerifyRetryTimerDelay(this, delay1)),
          WithArg<2>(sessions::test_util::SimulateGuRetryDelayCommand(delay2)),
          RecordSyncShare(&times, true)));

  // Run nudge GU.
  RunLoop();
  EXPECT_EQ(delay2, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));

  // Run to wait for retrying.
  RunLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerTest, ScheduleClearServerData_Succeeds) {
  StartSyncConfiguration();
  scheduler()->Start(SyncScheduler::CLEAR_SERVER_DATA_MODE, base::Time());
  CallbackCounter success_counter;
  ClearParams params(base::Bind(&CallbackCounter::Callback,
                                base::Unretained(&success_counter)));
  scheduler()->ScheduleClearServerData(params);
  PumpLoop();
  ASSERT_EQ(1, success_counter.times_called());
}

TEST_F(SyncSchedulerTest, ScheduleClearServerData_FailsRetriesSucceeds) {
  UseMockDelayProvider();
  TimeDelta delta(TimeDelta::FromMilliseconds(20));
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(delta));

  StartSyncConfiguration();
  scheduler()->Start(SyncScheduler::CLEAR_SERVER_DATA_MODE, base::Time());
  CallbackCounter success_counter;
  ClearParams params(base::Bind(&CallbackCounter::Callback,
                                base::Unretained(&success_counter)));

  // Next request will fail.
  connection()->SetServerNotReachable();
  scheduler()->ScheduleClearServerData(params);
  PumpLoop();
  ASSERT_EQ(0, success_counter.times_called());
  ASSERT_TRUE(scheduler()->IsBackingOff());

  // Now succeed.
  connection()->SetServerReachable();
  PumpLoopFor(2 * delta);
  ASSERT_EQ(1, success_counter.times_called());
  ASSERT_FALSE(scheduler()->IsBackingOff());
}

}  // namespace syncer
