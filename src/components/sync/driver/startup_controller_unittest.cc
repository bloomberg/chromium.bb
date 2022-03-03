// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/startup_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class StartupControllerTest : public testing::Test {
 public:
  StartupControllerTest() = default;

  void SetUp() override {
    policy_service_ = CreatePolicyService();
    controller_ = std::make_unique<StartupController>(
        base::BindRepeating(&StartupControllerTest::GetPreferredDataTypes,
                            base::Unretained(this)),
        base::BindRepeating(&StartupControllerTest::ShouldStart,
                            base::Unretained(this)),
        base::BindRepeating(&StartupControllerTest::FakeStartBackend,
                            base::Unretained(this)),
        policy_service_.get());
    controller_->Reset();
  }

  virtual std::unique_ptr<policy::PolicyService> CreatePolicyService() {
    auto policy_service =
        std::make_unique<testing::NiceMock<policy::MockPolicyService>>();
    ON_CALL(*policy_service, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    return policy_service;
  }

  void SetPreferredDataTypes(const ModelTypeSet& types) {
    preferred_types_ = types;
  }

  void SetShouldStart(bool should_start) { should_start_ = should_start; }

  void ExpectStarted() {
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(started());
    EXPECT_EQ(StartupController::State::STARTED, controller()->GetState());
  }

  void ExpectStartDeferred() {
    task_environment_.RunUntilIdle();
    EXPECT_FALSE(started());
    EXPECT_EQ(StartupController::State::STARTING_DEFERRED,
              controller()->GetState());
  }

  void ExpectNotStarted() {
    task_environment_.RunUntilIdle();
    EXPECT_FALSE(started());
    EXPECT_EQ(StartupController::State::NOT_STARTED, controller()->GetState());
  }

  bool started() const { return started_; }
  void clear_started() { started_ = false; }
  StartupController* controller() { return controller_.get(); }
  policy::PolicyService* policy_service() { return policy_service_.get(); }

  void RunDeferredTasks() { task_environment_.FastForwardUntilNoTasksRemain(); }

 private:
  ModelTypeSet GetPreferredDataTypes() { return preferred_types_; }
  bool ShouldStart() { return should_start_; }
  void FakeStartBackend() { started_ = true; }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ModelTypeSet preferred_types_ = UserTypes();
  bool should_start_ = false;
  bool started_ = false;
  std::unique_ptr<policy::PolicyService> policy_service_;
  std::unique_ptr<StartupController> controller_;
};

// Test that sync doesn't start if the "should sync start" callback returns
// false.
TEST_F(StartupControllerTest, ShouldNotStart) {
  controller()->TryStart(/*force_immediate=*/false);
  ExpectNotStarted();

  controller()->TryStart(/*force_immediate=*/true);
  ExpectNotStarted();
}

TEST_F(StartupControllerTest, DefersByDefault) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);
  ExpectStartDeferred();
}

// Test that a data type triggering startup starts sync immediately.
TEST_F(StartupControllerTest, NoDeferralDataTypeTrigger) {
  SetShouldStart(true);
  controller()->OnDataTypeRequestsSyncStartup(SESSIONS);
  ExpectStarted();
}

// Test that a data type trigger interrupts the deferral timer and starts
// sync immediately.
TEST_F(StartupControllerTest, DataTypeTriggerInterruptsDeferral) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);
  ExpectStartDeferred();

  controller()->OnDataTypeRequestsSyncStartup(SESSIONS);
  ExpectStarted();

  // The fallback timer shouldn't result in another invocation of the closure
  // we passed to the StartupController.
  clear_started();
  RunDeferredTasks();
  EXPECT_FALSE(started());
}

// Test that the fallback timer starts sync in the event all conditions are met
// and no data type requests sync.
TEST_F(StartupControllerTest, FallbackTimer) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);
  ExpectStartDeferred();

  RunDeferredTasks();
  ExpectStarted();
}

// Test that we start immediately if sessions is disabled.
TEST_F(StartupControllerTest, NoDeferralWithoutSessionsSync) {
  ModelTypeSet types(UserTypes());
  // Disabling sessions means disabling 4 types due to groupings.
  types.Remove(SESSIONS);
  types.Remove(PROXY_TABS);
  types.Remove(TYPED_URLS);
  types.Remove(SUPERVISED_USER_SETTINGS);
  SetPreferredDataTypes(types);

  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);
  ExpectStarted();
}

// Sanity check that the fallback timer doesn't fire before startup
// conditions are met.
TEST_F(StartupControllerTest, FallbackTimerWaits) {
  controller()->TryStart(/*force_immediate=*/false);
  ExpectNotStarted();
  RunDeferredTasks();
  ExpectNotStarted();
}

// Test that sync starts immediately when told to do so.
TEST_F(StartupControllerTest, NoDeferralIfForceImmediate) {
  SetShouldStart(true);
  ExpectNotStarted();
  controller()->TryStart(/*force_immediate=*/true);
  ExpectStarted();
}

// Test that setting |force_immediate| interrupts the deferral timer and starts
// sync immediately.
TEST_F(StartupControllerTest, ForceImmediateInterruptsDeferral) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);
  ExpectStartDeferred();

  controller()->TryStart(/*force_immediate=*/true);
  ExpectStarted();
}

// Tests the startup controller with a throttled PolicyService to simulate the
// wait for policies load.
class StartupControllerThrottledPolicyInitializationTest
    : public StartupControllerTest {
 public:
  std::unique_ptr<policy::PolicyService> CreatePolicyService() override {
    return policy::PolicyServiceImpl::CreateWithThrottledInitialization({});
  }

  void TriggerPolicyLoad() {
    static_cast<policy::PolicyServiceImpl*>(policy_service())
        ->UnthrottleInitialization();
  }
};

TEST_F(StartupControllerThrottledPolicyInitializationTest,
       PoliciesLoadedBeforeTryStart) {
  SetShouldStart(true);
  ExpectNotStarted();
  EXPECT_FALSE(controller()->ArePoliciesReady());

  // Policies are ready, but TryStart() hasn't been called yet.
  TriggerPolicyLoad();
  ExpectNotStarted();
  EXPECT_TRUE(controller()->ArePoliciesReady());

  controller()->TryStart(/*force_immediate=*/true);
  ExpectStarted();
}

TEST_F(StartupControllerThrottledPolicyInitializationTest,
       PoliciesLoadedAfterMultipleTryStart) {
  SetShouldStart(true);
  // Once TryStart is called with |force_immediate| set to true,
  // after the policies are loaded, the engine will bypass any deferred start.
  controller()->TryStart(/*force_immediate=*/false);
  controller()->TryStart(/*force_immediate=*/true);

  // Still waiting for policies, so sync startup is deferred.
  ExpectStartDeferred();
  EXPECT_FALSE(controller()->ArePoliciesReady());

  TriggerPolicyLoad();

  // Once policies are ready, sync startup is triggered automatically.
  EXPECT_TRUE(controller()->ArePoliciesReady());
  ExpectStarted();
}

TEST_F(StartupControllerThrottledPolicyInitializationTest,
       PoliciesLoadedAfterTryStart) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/true);

  // Still waiting for policies, so sync startup is deferred.
  ExpectStartDeferred();
  EXPECT_FALSE(controller()->ArePoliciesReady());

  TriggerPolicyLoad();

  // Once policies are ready, sync startup is triggered automatically.
  EXPECT_TRUE(controller()->ArePoliciesReady());
  ExpectStarted();
}

TEST_F(StartupControllerThrottledPolicyInitializationTest,
       PoliciesLoadTimeout) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/true);

  // Still waiting for policies, so sync startup is deferred.
  ExpectStartDeferred();
  EXPECT_FALSE(controller()->ArePoliciesReady());

  controller()->TriggerPolicyWaitTimeoutForTest();

  // Once the wait for policies times out, sync startup is triggered
  // automatically.
  EXPECT_TRUE(controller()->ArePoliciesReady());
  ExpectStarted();
}

TEST_F(StartupControllerThrottledPolicyInitializationTest,
       PoliciesLoadedAfterTryStartDeferred) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);
  ExpectStartDeferred();
  EXPECT_FALSE(controller()->ArePoliciesReady());

  TriggerPolicyLoad();

  // Once policies are ready, sync startup is triggered automatically.
  // Deferred start because TryStart was never called with |force_immediate| set
  // to true.
  EXPECT_TRUE(controller()->ArePoliciesReady());
  ExpectStartDeferred();
}

TEST_F(StartupControllerThrottledPolicyInitializationTest,
       PoliciesLoadTimeoutDeferredStart) {
  SetShouldStart(true);
  controller()->TryStart(/*force_immediate=*/false);

  // Still waiting for policies, so sync startup is deferred.
  ExpectStartDeferred();
  EXPECT_FALSE(controller()->ArePoliciesReady());

  controller()->TriggerPolicyWaitTimeoutForTest();

  // Once the wait for policies times out, sync startup is triggered
  // automatically. Deferred start because TryStart was never called with
  // |force_immediate| set to true.
  EXPECT_TRUE(controller()->ArePoliciesReady());
  ExpectStartDeferred();
}

}  // namespace syncer
