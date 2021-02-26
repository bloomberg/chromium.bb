// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_stability_monitor.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_chunneld_client.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniStabilityMonitorTest : public testing::Test {
 public:
  CrostiniStabilityMonitorTest() : task_env_() {
    chromeos::DBusThreadManager::Initialize();

    // CrostiniManager will create a CrostiniStabilityMonitor for us.
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = std::make_unique<CrostiniManager>(profile_.get());
    CrostiniTestHelper::EnableCrostini(profile_.get());

    // When CrostiniStabilityMonitor is initialized, it waits for the DBus
    // services to become available before monitoring them. In tests this
    // happens instantly, but the notification still comes via a callback on the
    // task queue, so run all queued tasks here.
    FlushTaskQueue();

    histogram_tester_.ExpectTotalCount(kCrostiniStabilityHistogram, 0);
  }

  ~CrostiniStabilityMonitorTest() override {
    CrostiniTestHelper::DisableCrostini(profile_.get());
    crostini_manager_->OnDBusShuttingDownForTesting();
    chromeos::DBusThreadManager::Shutdown();
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

  void SendVmStoppedSignal() {
    auto* concierge_client = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());

    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name("termina");
    signal.set_owner_id(CryptohomeIdForProfile(profile_.get()));
    concierge_client->NotifyVmStopped(signal);
  }

 protected:
  // CrostiniManager requires a full browser task environment to run.
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniManager> crostini_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(CrostiniStabilityMonitorTest, ConciergeFailure) {
  auto* concierge_client = static_cast<chromeos::FakeConciergeClient*>(
      chromeos::DBusThreadManager::Get()->GetConciergeClient());

  concierge_client->NotifyConciergeStopped();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::ConciergeStopped, 1);

  concierge_client->NotifyConciergeStarted();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::ConciergeStopped, 1);
}

TEST_F(CrostiniStabilityMonitorTest, CiceroneFailure) {
  auto* cicerone_client = static_cast<chromeos::FakeCiceroneClient*>(
      chromeos::DBusThreadManager::Get()->GetCiceroneClient());

  cicerone_client->NotifyCiceroneStopped();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::CiceroneStopped, 1);

  cicerone_client->NotifyCiceroneStarted();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::CiceroneStopped, 1);
}

TEST_F(CrostiniStabilityMonitorTest, SeneschalFailure) {
  auto* seneschal_client = static_cast<chromeos::FakeSeneschalClient*>(
      chromeos::DBusThreadManager::Get()->GetSeneschalClient());

  seneschal_client->NotifySeneschalStopped();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::SeneschalStopped, 1);

  seneschal_client->NotifySeneschalStarted();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::SeneschalStopped, 1);
}

TEST_F(CrostiniStabilityMonitorTest, ChunneldFailure) {
  auto* chunneld_client = static_cast<chromeos::FakeChunneldClient*>(
      chromeos::DBusThreadManager::Get()->GetChunneldClient());

  chunneld_client->NotifyChunneldStopped();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::ChunneldStopped, 1);

  chunneld_client->NotifyChunneldStarted();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::ChunneldStopped, 1);
}

TEST_F(CrostiniStabilityMonitorTest, UnknownVmStopped) {
  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 0);
}

TEST_F(CrostiniStabilityMonitorTest, VmStoppedDuringStartup) {
  crostini_manager_->AddRunningVmForTesting("termina");
  crostini_manager_->UpdateVmState("termina", VmState::STARTING);

  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 0);
}

TEST_F(CrostiniStabilityMonitorTest, ExpectedVmStopped) {
  crostini_manager_->AddStoppingVmForTesting("termina");

  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 0);
}

TEST_F(CrostiniStabilityMonitorTest, UnexpectedVmStopped) {
  crostini_manager_->AddRunningVmForTesting("termina");

  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 1);
}

}  // namespace crostini
