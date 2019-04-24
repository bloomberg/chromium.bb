// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestUpgradeDetectorChromeos : public UpgradeDetectorChromeos {
 public:
  explicit TestUpgradeDetectorChromeos(const base::Clock* clock,
                                       const base::TickClock* tick_clock)
      : UpgradeDetectorChromeos(clock, tick_clock) {}
  ~TestUpgradeDetectorChromeos() override = default;

  // Exposed for testing.
  using UpgradeDetectorChromeos::UPGRADE_AVAILABLE_REGULAR;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeDetectorChromeos);
};

class MockUpgradeObserver : public UpgradeObserver {
 public:
  explicit MockUpgradeObserver(UpgradeDetector* upgrade_detector)
      : upgrade_detector_(upgrade_detector) {
    upgrade_detector_->AddObserver(this);
  }
  ~MockUpgradeObserver() override { upgrade_detector_->RemoveObserver(this); }
  MOCK_METHOD0(OnUpdateOverCellularAvailable, void());
  MOCK_METHOD0(OnUpdateOverCellularOneTimePermissionGranted, void());
  MOCK_METHOD0(OnUpgradeRecommended, void());
  MOCK_METHOD0(OnCriticalUpgradeInstalled, void());
  MOCK_METHOD0(OnOutdatedInstall, void());
  MOCK_METHOD0(OnOutdatedInstallNoAutoUpdate, void());

 private:
  UpgradeDetector* const upgrade_detector_;
  DISALLOW_COPY_AND_ASSIGN(MockUpgradeObserver);
};

}  // namespace

class UpgradeDetectorChromeosTest : public ::testing::Test {
 protected:
  UpgradeDetectorChromeosTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()) {
    // Disable the detector's check to see if autoupdates are inabled.
    // Without this, tests put the detector into an invalid state by detecting
    // upgrades before the detection task completes.
    scoped_local_state_.Get()->SetUserPref(prefs::kAttemptedToEnableAutoupdate,
                                           std::make_unique<base::Value>(true));

    fake_update_engine_client_ = new chromeos::FakeUpdateEngineClient();
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<chromeos::UpdateEngineClient>(
            fake_update_engine_client_));
  }

  const base::Clock* GetMockClock() {
    return scoped_task_environment_.GetMockClock();
  }

  const base::TickClock* GetMockTickClock() {
    return scoped_task_environment_.GetMockTickClock();
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  void NotifyUpdateReadyToInstall() {
    chromeos::UpdateEngineClient::Status status;
    status.status =
        chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  // Fast-forwards virtual time by |delta|.
  void FastForwardBy(base::TimeDelta delta) {
    scoped_task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ScopedTestingLocalState scoped_local_state_;

  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorChromeosTest);
};

TEST_F(UpgradeDetectorChromeosTest, TestHighAnnoyanceDeadline) {
  TestUpgradeDetectorChromeos upgrade_detector(GetMockClock(),
                                               GetMockTickClock());
  upgrade_detector.Init();
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Observer should get some notifications about new version.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended()).Times(testing::AtLeast(1));
  NotifyUpdateReadyToInstall();

  const auto deadline = upgrade_detector.GetHighAnnoyanceDeadline();

  // Another new version of ChromeOS is ready to install after a day.
  FastForwardBy(base::TimeDelta::FromDays(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  // New notification could be sent or not.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended())
      .Times(testing::AnyNumber());
  NotifyUpdateReadyToInstall();

  // Deadline wasn't changed because of new upgrade detected.
  EXPECT_EQ(upgrade_detector.GetHighAnnoyanceDeadline(), deadline);
  upgrade_detector.Shutdown();
  RunUntilIdle();
}
