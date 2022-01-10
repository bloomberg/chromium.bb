// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/borealis_window_manager_test_helper.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/dbus.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/chunneld/fake_chunneld_client.h"
#include "chromeos/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/dbus/concierge/fake_concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal/fake_seneschal_client.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {

class BorealisContextTest : public testing::Test,
                            protected FakeVmServicesHelper {
 public:
  BorealisContextTest()
      : new_window_provider_(std::make_unique<ash::TestNewWindowDelegate>()) {
    profile_ = std::make_unique<TestingProfile>();
    borealis_disk_manager_dispatcher_ =
        std::make_unique<BorealisDiskManagerDispatcher>();
    borealis_shutdown_monitor_ =
        std::make_unique<BorealisShutdownMonitor>(profile_.get());
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(profile_.get());

    service_fake_ = BorealisServiceFake::UseFakeForTesting(profile_.get());
    service_fake_->SetDiskManagerDispatcherForTesting(
        borealis_disk_manager_dispatcher_.get());
    service_fake_->SetShutdownMonitorForTesting(
        borealis_shutdown_monitor_.get());
    service_fake_->SetWindowManagerForTesting(borealis_window_manager_.get());

    borealis_context_ =
        BorealisContext::CreateBorealisContextForTesting(profile_.get());

    // When GuestOsStabilityMonitor is initialized, it waits for the DBus
    // services to become available before monitoring them. In tests this
    // happens instantly, but the notification still comes via a callback on the
    // task queue, so run all queued tasks here.
    FlushTaskQueue();

    histogram_tester_.ExpectTotalCount(kBorealisStabilityHistogram, 0);
  }

  ~BorealisContextTest() override {
    borealis_context_.reset();  // must destroy before DBusThreadManager
  }

  // Run all tasks queued prior to this method being called, but not any tasks
  // that are scheduled as a result of those tasks running. This is done by
  // placing a quit closure at the current end of the queue and running until we
  // hit it.
  void FlushTaskQueue() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<borealis::BorealisContext> borealis_context_;
  std::unique_ptr<TestingProfile> profile_;
  BorealisServiceFake* service_fake_;
  std::unique_ptr<BorealisDiskManagerDispatcher>
      borealis_disk_manager_dispatcher_;
  std::unique_ptr<BorealisShutdownMonitor> borealis_shutdown_monitor_;
  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  base::HistogramTester histogram_tester_;
  ash::TestNewWindowDelegateProvider new_window_provider_;
};

TEST_F(BorealisContextTest, ConciergeFailure) {
  auto* concierge_client = chromeos::FakeConciergeClient::Get();

  concierge_client->NotifyConciergeStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ConciergeStopped,
      1);

  concierge_client->NotifyConciergeStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ConciergeStopped,
      1);
}

TEST_F(BorealisContextTest, CiceroneFailure) {
  auto* cicerone_client = chromeos::FakeCiceroneClient::Get();

  cicerone_client->NotifyCiceroneStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::CiceroneStopped,
      1);

  cicerone_client->NotifyCiceroneStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::CiceroneStopped,
      1);
}

TEST_F(BorealisContextTest, SeneschalFailure) {
  auto* seneschal_client = chromeos::FakeSeneschalClient::Get();

  seneschal_client->NotifySeneschalStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::SeneschalStopped,
      1);

  seneschal_client->NotifySeneschalStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::SeneschalStopped,
      1);
}

TEST_F(BorealisContextTest, ChunneldFailure) {
  auto* chunneld_client = static_cast<chromeos::FakeChunneldClient*>(
      chromeos::DBusThreadManager::Get()->GetChunneldClient());

  chunneld_client->NotifyChunneldStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ChunneldStopped,
      1);

  chunneld_client->NotifyChunneldStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ChunneldStopped,
      1);
}

TEST_F(BorealisContextTest, MainAppHasSelfActivationPermission) {
  CreateFakeMainApp(profile_.get());
  std::string window_name;
  ASSERT_TRUE(base::Base64Decode(
      "b3JnLmNocm9taXVtLmJvcmVhbGlzLndtY2xhc3MuU3RlYW0=", &window_name));
  std::unique_ptr<ScopedTestWindow> window =
      MakeAndTrackWindow(window_name, borealis_window_manager_.get());
  EXPECT_TRUE(exo::HasPermissionToActivate(window->window()));
}

TEST_F(BorealisContextTest, NormalAppDoesNotHaveSelfActivationPermission) {
  CreateFakeApp(profile_.get(), "some_app", "borealis/123");
  std::unique_ptr<ScopedTestWindow> window = MakeAndTrackWindow(
      "org.chromium.borealis.wmclass.some_app", borealis_window_manager_.get());
  EXPECT_FALSE(exo::HasPermissionToActivate(window->window()));
}

}  // namespace borealis
