// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_updater_chromeos.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/mock_build_state_observer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/update_engine/update_engine.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Property;

class InstalledVersionUpdaterTest : public ::testing::Test {
 protected:
  InstalledVersionUpdaterTest() {
    // Create the fake update engine client, hold a pointer to it, and hand
    // ownership of it off to the DBus thread manager.
    auto fake_update_engine_client =
        std::make_unique<chromeos::FakeUpdateEngineClient>();
    fake_update_engine_client_ = fake_update_engine_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::move(fake_update_engine_client));

    build_state_.AddObserver(&mock_observer_);
  }

  ~InstalledVersionUpdaterTest() override {
    build_state_.RemoveObserver(&mock_observer_);

    // Be kind; rewind.
    chromeos::DBusThreadManager::Shutdown();
  }

  void NotifyStatusChanged(update_engine::StatusResult status) {
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  base::test::TaskEnvironment task_environment_;
  ::testing::StrictMock<MockBuildStateObserver> mock_observer_;
  BuildState build_state_;

 private:
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;  // Not owned.
};

// Tests that an unrelated status change notification does not push data to the
// BuildState.
TEST_F(InstalledVersionUpdaterTest, UnrelatedStatus) {
  InstalledVersionUpdater installed_version_updater(&build_state_);
  update_engine::StatusResult status;

  status.set_current_operation(update_engine::NEED_PERMISSION_TO_UPDATE);
  NotifyStatusChanged(std::move(status));
}

// Tests that an update notifies the BuildState appropriately.
TEST_F(InstalledVersionUpdaterTest, Update) {
  InstalledVersionUpdater installed_version_updater(&build_state_);
  update_engine::StatusResult status;
  const std::string new_version("1.2.3");

  status.set_current_operation(update_engine::UPDATED_NEED_REBOOT);
  status.set_new_version(new_version);
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(Eq(&build_state_),
                     Property(&BuildState::update_type,
                              Eq(BuildState::UpdateType::kNormalUpdate)),
                     Property(&BuildState::installed_version, IsTrue()),
                     Property(&BuildState::installed_version,
                              Eq(base::Optional<base::Version>(
                                  base::Version(new_version)))),
                     Property(&BuildState::critical_version, IsFalse()))));
  NotifyStatusChanged(std::move(status));
}

// Tests that a rollback without channel change notifies the BuildState
// appropriately.
TEST_F(InstalledVersionUpdaterTest, Rollback) {
  InstalledVersionUpdater installed_version_updater(&build_state_);
  update_engine::StatusResult status;
  const std::string new_version("1.2.3");

  status.set_current_operation(update_engine::UPDATED_NEED_REBOOT);
  status.set_new_version(new_version);
  status.set_is_enterprise_rollback(true);
  EXPECT_CALL(
      mock_observer_,
      OnUpdate(AllOf(Eq(&build_state_),
                     Property(&BuildState::update_type,
                              Eq(BuildState::UpdateType::kEnterpriseRollback)),
                     Property(&BuildState::installed_version, IsTrue()),
                     Property(&BuildState::installed_version,
                              Eq(base::Optional<base::Version>(
                                  base::Version(new_version)))),
                     Property(&BuildState::critical_version, IsFalse()))));
  NotifyStatusChanged(std::move(status));
  task_environment_.RunUntilIdle();
}
