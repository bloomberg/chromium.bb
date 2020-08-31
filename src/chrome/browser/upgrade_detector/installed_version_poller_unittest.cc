// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_poller.h"

#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/mock_build_state_observer.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Property;
using ::testing::Return;

class InstalledVersionPollerTest : public ::testing::Test {
 protected:
  InstalledVersionPollerTest() { build_state_.AddObserver(&mock_observer_); }
  ~InstalledVersionPollerTest() override {
    build_state_.RemoveObserver(&mock_observer_);
  }

  // Returns a version somewhat higher than the running version.
  static base::Version GetUpgradeVersion() {
    std::vector<uint32_t> components = version_info::GetVersion().components();
    components[3] += 2;
    return base::Version(components);
  }

  // Returns a version between the running version and the upgrade version
  // above.
  static base::Version GetCriticalVersion() {
    std::vector<uint32_t> components = version_info::GetVersion().components();
    components[3] += 1;
    return base::Version(components);
  }

  // Returns a version lower than the running version.
  static base::Version GetRollbackVersion() {
    std::vector<uint32_t> components = version_info::GetVersion().components();
    components[0] -= 1;
    return base::Version(components);
  }

  // Returns an InstalledAndCriticalVersion instance indicating no update.
  static InstalledAndCriticalVersion MakeNoUpdateVersions() {
    return InstalledAndCriticalVersion(version_info::GetVersion());
  }

  // Returns an InstalledAndCriticalVersion instance indicating an upgrade.
  static InstalledAndCriticalVersion MakeUpgradeVersions() {
    return InstalledAndCriticalVersion(GetUpgradeVersion());
  }

  // Returns an InstalledAndCriticalVersion instance indicating an upgrade with
  // a critical version.
  static InstalledAndCriticalVersion MakeCriticalUpgradeVersions() {
    return InstalledAndCriticalVersion(GetUpgradeVersion(),
                                       GetCriticalVersion());
  }

  // Returns an InstalledAndCriticalVersion instance with an invalid version.
  static InstalledAndCriticalVersion MakeErrorVersions() {
    return InstalledAndCriticalVersion(base::Version());
  }

  // Returns an InstalledAndCriticalVersion instance for a version rollback.
  static InstalledAndCriticalVersion MakeRollbackVersions() {
    return InstalledAndCriticalVersion(GetRollbackVersion());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::testing::StrictMock<MockBuildStateObserver> mock_observer_;
  BuildState build_state_;
};

// Tests that a poll returning the current version does not update the
// BuildState.
TEST_F(InstalledVersionPollerTest, TestNoUpdate) {
  base::MockRepeatingCallback<InstalledAndCriticalVersion()> callback;
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeNoUpdateVersions())));
  InstalledVersionPoller poller(&build_state_, callback.Get(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // A second poll with the same version likewise does nothing.
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeNoUpdateVersions())));
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll with an update is reported to the BuildState.
TEST_F(InstalledVersionPollerTest, TestUpgrade) {
  base::MockRepeatingCallback<InstalledAndCriticalVersion()> callback;

  // No update the first time.
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeNoUpdateVersions())));
  InstalledVersionPoller poller(&build_state_, callback.Get(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Followed by an update, which is reported.
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeUpgradeVersions())));
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kNormalUpdate)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(base::Optional<base::Version>(GetUpgradeVersion()))),
          Property(&BuildState::critical_version, IsFalse()))));
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Followed by the same update, which is not reported.
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeUpgradeVersions())));
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll with an update is reported to the BuildState and that a
// subsequent poll back to the original version is also reported.
TEST_F(InstalledVersionPollerTest, TestUpgradeThenDowngrade) {
  base::MockRepeatingCallback<InstalledAndCriticalVersion()> callback;

  // An update is found.
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeUpgradeVersions())));
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kNormalUpdate)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(base::Optional<base::Version>(GetUpgradeVersion()))),
          Property(&BuildState::critical_version, IsFalse()))));
  InstalledVersionPoller poller(&build_state_, callback.Get(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Which is then reverted back to the running version.
  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeNoUpdateVersions())));
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type, Eq(BuildState::UpdateType::kNone)),
          Property(&BuildState::installed_version, IsFalse()),
          Property(&BuildState::critical_version, IsFalse()))));
  task_environment_.FastForwardBy(
      InstalledVersionPoller::kDefaultPollingInterval);

  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll with a critical update is reported to the BuildState.
TEST_F(InstalledVersionPollerTest, TestCriticalUpgrade) {
  base::MockRepeatingCallback<InstalledAndCriticalVersion()> callback;

  EXPECT_CALL(callback, Run())
      .WillOnce(Return(ByMove(MakeCriticalUpgradeVersions())));
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kNormalUpdate)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(base::Optional<base::Version>(GetUpgradeVersion()))),
          Property(&BuildState::critical_version, IsTrue()),
          Property(&BuildState::critical_version,
                   Eq(base::Optional<base::Version>(GetCriticalVersion()))))));
  InstalledVersionPoller poller(&build_state_, callback.Get(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a poll that failed to find a version reports an update anyway.
TEST_F(InstalledVersionPollerTest, TestMissingVersion) {
  base::MockRepeatingCallback<InstalledAndCriticalVersion()> callback;

  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeErrorVersions())));
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(Eq(&build_state_),
                     Property(&BuildState::update_type,
                              Eq(BuildState::UpdateType::kNormalUpdate)),
                     Property(&BuildState::installed_version, IsFalse()),
                     Property(&BuildState::critical_version, IsFalse()))));
  InstalledVersionPoller poller(&build_state_, callback.Get(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}

// Tests that a version downgrade (a rollback) is reported as such.
TEST_F(InstalledVersionPollerTest, TestRollback) {
  base::MockRepeatingCallback<InstalledAndCriticalVersion()> callback;

  EXPECT_CALL(callback, Run()).WillOnce(Return(ByMove(MakeRollbackVersions())));
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(
          Eq(&build_state_),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kEnterpriseRollback)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(base::Optional<base::Version>(GetRollbackVersion()))),
          Property(&BuildState::critical_version, IsFalse()))));
  InstalledVersionPoller poller(&build_state_, callback.Get(),
                                task_environment_.GetMockTickClock());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);
}
