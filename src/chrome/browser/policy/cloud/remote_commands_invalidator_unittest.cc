// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/remote_commands_invalidator.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/mock_ack_handler.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::Mock;
using ::testing::StrictMock;

namespace {

MATCHER_P(InvalidationsEqual, expected_invalidation, "") {
  return arg.Equals(expected_invalidation);
}

}  // namespace

namespace policy {

class MockRemoteCommandInvalidator : public RemoteCommandsInvalidator {
 public:
  MockRemoteCommandInvalidator() {}

  MOCK_METHOD0(OnInitialize, void());
  MOCK_METHOD0(OnShutdown, void());
  MOCK_METHOD0(OnStart, void());
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(DoRemoteCommandsFetch, void(const syncer::Invalidation&));

  void SetInvalidationTopic(const syncer::Topic& topic) {
    em::PolicyData policy_data;
    policy_data.set_command_invalidation_topic(topic);
    ReloadPolicyData(&policy_data);
  }

  void ClearInvalidationTopic() {
    const em::PolicyData policy_data;
    ReloadPolicyData(&policy_data);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRemoteCommandInvalidator);
};

class RemoteCommandsInvalidatorTest : public testing::Test {
 public:
  RemoteCommandsInvalidatorTest()
      : kTestingTopic1("abcdef"), kTestingTopic2("defabc") {}

  void EnableInvalidationService() {
    invalidation_service_.SetInvalidatorState(syncer::INVALIDATIONS_ENABLED);
  }

  void DisableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        syncer::TRANSIENT_INVALIDATION_ERROR);
  }

  syncer::Invalidation CreateInvalidation(const syncer::Topic& topic) {
    return syncer::Invalidation::InitUnknownVersion(topic);
  }

  syncer::Invalidation FireInvalidation(const syncer::Topic& topic) {
    const syncer::Invalidation invalidation = CreateInvalidation(topic);
    invalidation_service_.EmitInvalidationForTest(invalidation);
    return invalidation;
  }

  bool IsInvalidationSent(const syncer::Invalidation& invalidation) {
    return !invalidation_service_.GetMockAckHandler()->IsUnsent(invalidation);
  }

  bool IsInvalidationAcknowledged(const syncer::Invalidation& invalidation) {
    return invalidation_service_.GetMockAckHandler()->IsAcknowledged(
        invalidation);
  }

  bool IsInvalidatorRegistered() {
    return !invalidation_service_.invalidator_registrar()
                .GetRegisteredTopics(&invalidator_)
                .empty();
  }

  void VerifyExpectations() {
    Mock::VerifyAndClearExpectations(&invalidator_);
  }

 protected:
  // Initialize and start the invalidator.
  void InitializeAndStart() {
    EXPECT_CALL(invalidator_, OnInitialize()).Times(1);
    invalidator_.Initialize(&invalidation_service_);
    VerifyExpectations();

    EXPECT_CALL(invalidator_, OnStart()).Times(1);
    invalidator_.Start();

    VerifyExpectations();
  }

  // Stop and shutdown the invalidator.
  void StopAndShutdown() {
    EXPECT_CALL(invalidator_, OnStop()).Times(1);
    EXPECT_CALL(invalidator_, OnShutdown()).Times(1);
    invalidator_.Shutdown();

    VerifyExpectations();
  }

  // Fire an invalidation to verify that invalidation is not working.
  void VerifyInvalidationDisabled(const syncer::Topic& topic) {
    const syncer::Invalidation invalidation = FireInvalidation(topic);

    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(IsInvalidationSent(invalidation));
  }

  // Fire an invalidation to verify that invalidation works.
  void VerifyInvalidationEnabled(const syncer::Topic& topic) {
    EXPECT_TRUE(invalidator_.invalidations_enabled());

    EXPECT_CALL(
        invalidator_,
        DoRemoteCommandsFetch(InvalidationsEqual(CreateInvalidation(topic))))
        .Times(1);
    const syncer::Invalidation invalidation = FireInvalidation(topic);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(IsInvalidationSent(invalidation));
    EXPECT_TRUE(IsInvalidationAcknowledged(invalidation));
    VerifyExpectations();
  }

  syncer::Topic kTestingTopic1;
  syncer::Topic kTestingTopic2;

  base::test::SingleThreadTaskEnvironment task_environment_;

  invalidation::FakeInvalidationService invalidation_service_;
  StrictMock<MockRemoteCommandInvalidator> invalidator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCommandsInvalidatorTest);
};

// Verifies that only the fired invalidations will be received.
TEST_F(RemoteCommandsInvalidatorTest, FiredInvalidation) {
  InitializeAndStart();

  // Invalidator won't work at this point.
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  // Load the policy data, it should work now.
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  EXPECT_TRUE(invalidator_.invalidations_enabled());

  base::RunLoop().RunUntilIdle();
  // No invalidation will be received if no invalidation is fired.
  VerifyExpectations();

  // Fire an invalidation with different object id, no invalidation will be
  // received.
  const syncer::Invalidation invalidation1 = FireInvalidation(kTestingTopic2);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsInvalidationSent(invalidation1));
  VerifyExpectations();

  // Fire the invalidation, it should be acknowledged and trigger a remote
  // commands fetch.
  EXPECT_CALL(invalidator_, DoRemoteCommandsFetch(InvalidationsEqual(
                                CreateInvalidation(kTestingTopic1))))
      .Times(1);
  const syncer::Invalidation invalidation2 = FireInvalidation(kTestingTopic1);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsInvalidationSent(invalidation2));
  EXPECT_TRUE(IsInvalidationAcknowledged(invalidation2));
  VerifyExpectations();

  StopAndShutdown();
}

// Verifies that no invalidation will be received when invalidator is shutdown.
TEST_F(RemoteCommandsInvalidatorTest, ShutDown) {
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  FireInvalidation(kTestingTopic1);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invalidator_.invalidations_enabled());
}

// Verifies that no invalidation will be received when invalidator is stopped.
TEST_F(RemoteCommandsInvalidatorTest, Stopped) {
  EXPECT_CALL(invalidator_, OnInitialize()).Times(1);
  invalidator_.Initialize(&invalidation_service_);
  VerifyExpectations();

  EXPECT_FALSE(invalidator_.invalidations_enabled());
  FireInvalidation(kTestingTopic2);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EXPECT_CALL(invalidator_, OnShutdown()).Times(1);
  invalidator_.Shutdown();
}

// Verifies that stated/stopped state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, StartedStateChange) {
  InitializeAndStart();

  // Invalidator requires topic to work.
  VerifyInvalidationDisabled(kTestingTopic1);
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  // Stop and restart invalidator.
  EXPECT_CALL(invalidator_, OnStop()).Times(1);
  invalidator_.Stop();
  VerifyExpectations();

  VerifyInvalidationDisabled(kTestingTopic1);
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EXPECT_CALL(invalidator_, OnStart()).Times(1);
  invalidator_.Start();
  VerifyExpectations();

  // Invalidator requires topic to work.
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  StopAndShutdown();
}

// Verifies that registered state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, RegistedStateChange) {
  InitializeAndStart();

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  invalidator_.SetInvalidationTopic(kTestingTopic2);
  VerifyInvalidationEnabled(kTestingTopic2);
  VerifyInvalidationDisabled(kTestingTopic1);

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  VerifyInvalidationDisabled(kTestingTopic2);

  invalidator_.ClearInvalidationTopic();
  VerifyInvalidationDisabled(kTestingTopic1);
  VerifyInvalidationDisabled(kTestingTopic2);
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  invalidator_.SetInvalidationTopic(kTestingTopic2);
  VerifyInvalidationEnabled(kTestingTopic2);
  VerifyInvalidationDisabled(kTestingTopic1);

  StopAndShutdown();
}

// Verifies that invalidation service enabled state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, InvalidationServiceEnabledStateChanged) {
  InitializeAndStart();

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EnableInvalidationService();
  VerifyInvalidationEnabled(kTestingTopic1);

  EnableInvalidationService();
  VerifyInvalidationEnabled(kTestingTopic1);

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  StopAndShutdown();
}

}  // namespace policy
