// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_shell_controller.h"

#include <memory>

#include "ash/kiosk_next/kiosk_next_shell_observer.h"
#include "ash/kiosk_next/kiosk_next_shell_test_util.h"
#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

KioskNextShellController* GetKioskNextShellController() {
  return Shell::Get()->kiosk_next_shell_controller();
}

class MockKioskNextShellObserver : public KioskNextShellObserver {
 public:
  MockKioskNextShellObserver() = default;
  ~MockKioskNextShellObserver() override = default;

  MOCK_METHOD0(OnKioskNextEnabled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKioskNextShellObserver);
};

class KioskNextShellControllerTest : public AshTestBase {
 public:
  KioskNextShellControllerTest() = default;

  ~KioskNextShellControllerTest() override = default;

  void SetUp() override {
    set_start_session(false);
    AshTestBase::SetUp();
    client_ = BindMockKioskNextShellClient();
    scoped_feature_list_.InitAndEnableFeature(features::kKioskNextShell);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    client_.reset();
  }

 protected:
  std::unique_ptr<MockKioskNextShellClient> client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskNextShellControllerTest);
};

// Ensures that KioskNextShell is not enabled when conditions are not met.
TEST_F(KioskNextShellControllerTest, TestKioskNextNotEnabled) {
  // No user has logged in yet.
  EXPECT_FALSE(GetKioskNextShellController()->IsEnabled());

  SimulateUserLogin("primary_user1@test.com");

  // KioskNextShell is not enabled for regular users by default.
  EXPECT_FALSE(GetKioskNextShellController()->IsEnabled());
}

// Ensures that KioskNextShell is enabled when conditions are met.
// Ensures that LaunchKioskNextShell is called when KioskNextUser logs in.
TEST_F(KioskNextShellControllerTest, TestKioskNextLaunchShellWhenEnabled) {
  EXPECT_CALL(*client_, LaunchKioskNextShell(::testing::_)).Times(1);
  LogInKioskNextUser(GetSessionControllerClient());
  EXPECT_TRUE(GetKioskNextShellController()->IsEnabled());
  histogram_tester_.ExpectUniqueSample("KioskNextShell.Launched", true, 1);
}

// Ensures that observers are notified when KioskNextUser logs in.
TEST_F(KioskNextShellControllerTest, TestKioskNextObserverNotification) {
  auto observer = std::make_unique<MockKioskNextShellObserver>();
  GetKioskNextShellController()->AddObserver(observer.get());
  EXPECT_CALL(*observer, OnKioskNextEnabled).Times(1);
  LogInKioskNextUser(GetSessionControllerClient());
  GetKioskNextShellController()->RemoveObserver(observer.get());
}

}  // namespace
}  // namespace ash
