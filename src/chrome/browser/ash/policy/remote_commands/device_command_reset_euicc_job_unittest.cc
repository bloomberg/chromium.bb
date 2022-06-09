// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reset_euicc_job.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace em = enterprise_management;

namespace {

const char kTestEuiccPath[] = "/test/hermes/123456789";
const char kTestEid[] = "123456789";
const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
const char kResetEuiccOperationResultHistogram[] =
    "Network.Cellular.ESim.Policy.ResetEuicc.Result";
const char kResetEuiccDurationHistogram[] =
    "Network.Cellular.ESim.Policy.ResetEuicc.Duration";

em::RemoteCommand GenerateResetEuiccCommandProto(
    base::TimeDelta age_of_command) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_RESET_EUICC);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  return command_proto;
}

void VerifyEuiccProfileCount(int expected_count) {
  chromeos::HermesEuiccClient::Properties* euicc_properties =
      chromeos::HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kTestEuiccPath));
  EXPECT_EQ(expected_count,
            euicc_properties->installed_carrier_profiles().value().size());
}

void VerifyJobResult(base::RunLoop* run_loop,
                     RemoteCommandJob* job,
                     RemoteCommandJob::Status expected_status,
                     int expected_profile_count) {
  EXPECT_EQ(expected_status, job->status());
  VerifyEuiccProfileCount(expected_profile_count);
  run_loop->Quit();
}

}  // namespace

class DeviceCommandResetEuiccJobTest : public ChromeAshTestBase {
 public:
  DeviceCommandResetEuiccJobTest() {
    cellular_inhibitor_->Init(helper_.network_state_handler(),
                              helper_.network_device_handler());
  }
  DeviceCommandResetEuiccJobTest(const DeviceCommandResetEuiccJobTest&) =
      delete;
  DeviceCommandResetEuiccJobTest& operator=(
      const DeviceCommandResetEuiccJobTest&) = delete;
  ~DeviceCommandResetEuiccJobTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid,
        /*is_active=*/true, /*physical_slot=*/0);

    AddFakeESimProfile();
    AddFakeESimProfile();
    // Wait for all pending Hermes and Shill change notifications to be handled
    // so that new EUICC and profile states are reflected correctly.
    base::RunLoop().RunUntilIdle();
    VerifyEuiccProfileCount(/*expected_count=*/2);
  }

 protected:
  std::unique_ptr<RemoteCommandJob> CreateResetEuiccJob(
      base::TimeTicks issued_time) {
    std::unique_ptr<DeviceCommandResetEuiccJob> job =
        DeviceCommandResetEuiccJob::CreateForTesting(cellular_inhibitor_.get());
    auto reset_euicc_command_proto =
        GenerateResetEuiccCommandProto(base::TimeTicks::Now() - issued_time);
    EXPECT_TRUE(job->Init(base::TimeTicks::Now(), reset_euicc_command_proto,
                          em::SignedData()));
    EXPECT_EQ(kUniqueID, job->unique_id());
    EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
    return job;
  }

  void AddFakeESimProfile() {
    helper_.hermes_euicc_test()->AddFakeCarrierProfile(
        dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive,
        /*activation_code=*/"",
        chromeos::HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
  }

  base::HistogramTester histogram_tester_;
  chromeos::NetworkStateTestHelper helper_{
      /*use_default_devices_and_services=*/true};
  std::unique_ptr<chromeos::CellularInhibitor> cellular_inhibitor_ =
      std::make_unique<chromeos::CellularInhibitor>();
  base::TimeTicks test_start_time_ = base::TimeTicks::Now();
};

TEST_F(DeviceCommandResetEuiccJobTest, ResetEuicc) {
  base::RunLoop run_loop;
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(/*profile=*/nullptr);

  std::unique_ptr<RemoteCommandJob> job = CreateResetEuiccJob(test_start_time_);
  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindOnce(&VerifyJobResult, base::Unretained(&run_loop),
                              base::Unretained(job.get()),
                              RemoteCommandJob::Status::SUCCEEDED,
                              /*expected_profile_count=*/0)));
  run_loop.Run();
  // Verify that the notification should be displayed.
  EXPECT_TRUE(tester.GetNotification(
      DeviceCommandResetEuiccJob::kResetEuiccNotificationId));
  // Verfiy that appropriate metrics have been logged.
  histogram_tester_.ExpectTotalCount(kResetEuiccOperationResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kResetEuiccOperationResultHistogram,
      DeviceCommandResetEuiccJob::ResetEuiccResult::kSuccess,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(kResetEuiccDurationHistogram, 1);
}

TEST_F(DeviceCommandResetEuiccJobTest, ResetEuiccInhibitFailure) {
  chromeos::ShillManagerClient::Get()->GetTestInterface()->ClearDevices();
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  NotificationDisplayServiceTester tester(/*profile=*/nullptr);
  base::RunLoop run_loop;
  std::unique_ptr<RemoteCommandJob> job = CreateResetEuiccJob(test_start_time_);
  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindOnce(&VerifyJobResult, base::Unretained(&run_loop),
                              base::Unretained(job.get()),
                              RemoteCommandJob::Status::FAILED,
                              /*expected_profile_count=*/2)));
  run_loop.Run();
  // Verify that the notification should be displayed.
  EXPECT_TRUE(tester.GetNotification(
      DeviceCommandResetEuiccJob::kResetEuiccNotificationId));
  // Verfiy that appropriate metrics have been logged.
  histogram_tester_.ExpectTotalCount(kResetEuiccOperationResultHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kResetEuiccOperationResultHistogram,
      DeviceCommandResetEuiccJob::ResetEuiccResult::kInhibitFailed,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(kResetEuiccDurationHistogram, 0);
}

}  // namespace policy
