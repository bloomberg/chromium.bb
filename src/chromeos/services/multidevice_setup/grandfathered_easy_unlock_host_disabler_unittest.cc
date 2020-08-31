// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"

#include <memory>

#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// Parameterized test types, indicating the following test scenarios:
enum class TestType {
  // Use v1 DeviceSync and host does not have an Instance ID.
  kYesV1NoInstanceId,
  // Use v1 DeviceSync and host has an Instance ID.
  kYesV1YesInstanceId,
  // Do not use v1 DeviceSync and host has an Instance ID.
  kNoV1YesInstanceId
};

const char kEasyUnlockHostIdToDisablePrefName[] =
    "multidevice_setup.easy_unlock_host_id_to_disable";
const char kEasyUnlockHostInstanceIdToDisablePrefName[] =
    "multidevice_setup.easy_unlock_host_instance_id_to_disable";

const char kNoDevice[] = "";

const size_t kNumTestDevices = 2;

}  // namespace

class MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest
    : public ::testing::TestWithParam<TestType> {
 protected:
  MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest() override = default;

  // testing::Test:
  void SetUp() override {
    SetDeviceSyncFeatureFlags();

    for (auto& device : test_devices_) {
      if (!HasInstanceId())
        GetMutableRemoteDevice(device)->instance_id.clear();

      // Don't rely on a legacy device ID if not using v1 DeviceSync, even
      // though we almost always expect one in practice.
      if (!features::ShouldUseV1DeviceSync())
        GetMutableRemoteDevice(device)->public_key.clear();
    }

    fake_host_backend_delegate_ = std::make_unique<FakeHostBackendDelegate>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    GrandfatheredEasyUnlockHostDisabler::RegisterPrefs(
        test_pref_service_->registry());
  }

  void SetHost(const base::Optional<multidevice::RemoteDeviceRef>& host_device,
               multidevice::SoftwareFeature host_type) {
    if (host_type != multidevice::SoftwareFeature::kBetterTogetherHost &&
        host_type != multidevice::SoftwareFeature::kSmartLockHost)
      return;

    for (const auto& remote_device : test_devices_) {
      bool should_be_host =
          host_device != base::nullopt &&
          host_device->GetDeviceId() == remote_device.GetDeviceId() &&
          host_device->instance_id() == remote_device.instance_id();

      GetMutableRemoteDevice(remote_device)->software_features[host_type] =
          should_be_host ? multidevice::SoftwareFeatureState::kEnabled
                         : multidevice::SoftwareFeatureState::kSupported;
    }

    if (host_type == multidevice::SoftwareFeature::kBetterTogetherHost)
      fake_host_backend_delegate_->NotifyHostChangedOnBackend(host_device);
  }

  void InitializeTest(
      base::Optional<multidevice::RemoteDeviceRef> initial_device_in_prefs,
      base::Optional<multidevice::RemoteDeviceRef> initial_better_together_host,
      base::Optional<multidevice::RemoteDeviceRef> initial_easy_unlock_host) {
    test_pref_service_->SetString(kEasyUnlockHostIdToDisablePrefName,
                                  initial_device_in_prefs
                                      ? initial_device_in_prefs->GetDeviceId()
                                      : kNoDevice);
    test_pref_service_->SetString(kEasyUnlockHostInstanceIdToDisablePrefName,
                                  initial_device_in_prefs
                                      ? initial_device_in_prefs->instance_id()
                                      : kNoDevice);

    SetHost(initial_better_together_host,
            multidevice::SoftwareFeature::kBetterTogetherHost);
    SetHost(initial_easy_unlock_host,
            multidevice::SoftwareFeature::kSmartLockHost);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    grandfathered_easy_unlock_host_disabler_ =
        GrandfatheredEasyUnlockHostDisabler::Factory::Create(
            fake_host_backend_delegate_.get(), fake_device_sync_client_.get(),
            test_pref_service_.get(), std::move(mock_timer));
  }

  // Verify that the IDs for |expected_device| are stored in prefs. If
  // |expected_device| is null, prefs should have value |kNoDevice|.
  void VerifyDeviceInPrefs(
      const base::Optional<multidevice::RemoteDeviceRef>& expected_device) {
    if (!expected_device) {
      EXPECT_EQ(kNoDevice, test_pref_service_->GetString(
                               kEasyUnlockHostIdToDisablePrefName));
      EXPECT_EQ(kNoDevice, test_pref_service_->GetString(
                               kEasyUnlockHostInstanceIdToDisablePrefName));
      return;
    }

    EXPECT_EQ(
        expected_device->GetDeviceId().empty() ? kNoDevice
                                               : expected_device->GetDeviceId(),
        test_pref_service_->GetString(kEasyUnlockHostIdToDisablePrefName));
    EXPECT_EQ(expected_device->instance_id().empty()
                  ? kNoDevice
                  : expected_device->instance_id(),
              test_pref_service_->GetString(
                  kEasyUnlockHostInstanceIdToDisablePrefName));
  }

  void VerifyEasyUnlockHostDisableRequest(
      int expected_queue_size,
      const base::Optional<multidevice::RemoteDeviceRef>& expected_host) {
    EXPECT_EQ(
        expected_queue_size,
        features::ShouldUseV1DeviceSync()
            ? fake_device_sync_client_
                  ->GetSetSoftwareFeatureStateInputsQueueSize()
            : fake_device_sync_client_->GetSetFeatureStatusInputsQueueSize());
    if (expected_queue_size > 0) {
      ASSERT_TRUE(expected_host);
      VerifyLatestEasyUnlockHostDisableRequest(*expected_host);
    }
  }

  void InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult result_code) {
    if (features::ShouldUseV1DeviceSync()) {
      fake_device_sync_client_->InvokePendingSetSoftwareFeatureStateCallback(
          result_code);
    } else {
      fake_device_sync_client_->InvokePendingSetFeatureStatusCallback(
          result_code);
    }
  }

  const multidevice::RemoteDeviceRefList& test_devices() const {
    return test_devices_;
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() const {
    return fake_device_sync_client_.get();
  }

  base::MockOneShotTimer* mock_timer() const { return mock_timer_; }

 private:
  bool HasInstanceId() {
    switch (GetParam()) {
      case TestType::kYesV1YesInstanceId:
        FALLTHROUGH;
      case TestType::kNoV1YesInstanceId:
        return true;
      case TestType::kYesV1NoInstanceId:
        return false;
    }
  }

  void SetDeviceSyncFeatureFlags() {
    bool use_v1;
    switch (GetParam()) {
      case TestType::kYesV1YesInstanceId:
        FALLTHROUGH;
      case TestType::kYesV1NoInstanceId:
        use_v1 = true;
        break;
      case TestType::kNoV1YesInstanceId:
        use_v1 = false;
        break;
    }
  }

  void VerifyLatestEasyUnlockHostDisableRequest(
      const multidevice::RemoteDeviceRef& expected_host) {
    // Verify inputs to SetSoftwareFeatureState().
    if (features::ShouldUseV1DeviceSync()) {
      ASSERT_FALSE(
          fake_device_sync_client_->set_software_feature_state_inputs_queue()
              .empty());
      const device_sync::FakeDeviceSyncClient::SetSoftwareFeatureStateInputs&
          inputs = fake_device_sync_client_
                       ->set_software_feature_state_inputs_queue()
                       .back();
      EXPECT_EQ(expected_host.public_key(), inputs.public_key);
      EXPECT_EQ(multidevice::SoftwareFeature::kSmartLockHost,
                inputs.software_feature);
      EXPECT_FALSE(inputs.enabled);
      EXPECT_FALSE(inputs.is_exclusive);
      return;
    }

    // Verify inputs to SetFeatureStatus().
    ASSERT_FALSE(
        fake_device_sync_client_->set_feature_status_inputs_queue().empty());
    const device_sync::FakeDeviceSyncClient::SetFeatureStatusInputs& inputs =
        fake_device_sync_client_->set_feature_status_inputs_queue().back();
    EXPECT_EQ(expected_host.instance_id(), inputs.device_instance_id);
    EXPECT_EQ(multidevice::SoftwareFeature::kSmartLockHost, inputs.feature);
    EXPECT_EQ(device_sync::FeatureStatusChange::kDisable, inputs.status_change);
  }

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeHostBackendDelegate> fake_host_backend_delegate_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  base::MockOneShotTimer* mock_timer_ = nullptr;

  std::unique_ptr<GrandfatheredEasyUnlockHostDisabler>
      grandfathered_easy_unlock_host_disabler_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest);
};

// Situation #1:
//   BTH = BETTER_TOGETHER_HOST, 0 = disabled, A = devices[0]
//   EUH = EASY_UNLOCK_HOST,     1 = enabled,  B = devices[1]
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 0 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 1 | 0 |
//
// Grandfathering prevents EUH from being disabled automatically. This class
// disables EUH manually.
TEST_P(
    MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
    IfBetterTogetherHostChangedFromOneDeviceToNoDeviceThenDisableEasyUnlock) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyDeviceInPrefs(test_devices()[0]);
  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);
  InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  EXPECT_FALSE(mock_timer()->IsRunning());
}

// Situation #2:
//   BTH = BETTER_TOGETHER_HOST, 0 = disabled, A = devices[0]
//   EUH = EASY_UNLOCK_HOST,     1 = enabled,  B = devices[1]
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 0 | 0 |        BTH| 0 | 1 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 0 | 1 |
//
// The CryptAuth backend (via GmsCore) disables EUH on device A when BTH is
// enabled (exclusively) on another device, B. No action necessary from this
// class.
TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfBetterTogetherHostChangedFromNoDeviceToADeviceThenDoNothing) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 base::nullopt /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(test_devices()[1], multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);
}

// Situation #3:
//   BTH = BETTER_TOGETHER_HOST, 0 = disabled, A = devices[0]
//   EUH = EASY_UNLOCK_HOST,     1 = enabled,  B = devices[1]
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 1 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 0 | 1 |
//
// The CryptAuth backend (via GmsCore) disables EUH on device A when BTH is
// enabled (exclusively) on another device, B. We still attempt to disable
// EUH in this case to be safe.
TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfBetterTogetherHostChangedFromOneDeviceToAnotherThenDisableEasyUnlock) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(test_devices()[1], multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyDeviceInPrefs(test_devices()[0]);
  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);
  InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDisablePendingThenConstructorAttemptsToDisableEasyUnlock) {
  InitializeTest(test_devices()[0] /* initial_device_in_prefs */,
                 base::nullopt /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  VerifyDeviceInPrefs(test_devices()[0]);
  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);
  InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);
}

// Situation #1 where device A is removed from list of synced devices:
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 0 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 1 | 0 |
TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfHostToDisableIsNotInListOfSyncedDevicesThenClearPref) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  // Remove device[0] from list
  fake_device_sync_client()->set_synced_devices({test_devices()[1]});

  SetHost(base::nullopt, multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);
}

// Situation #1 with failure:
//
//    | A | B |           | A | B |
// ---+---+---+        ---+---+---+
// BTH| 1 | 0 |        BTH| 0 | 0 |
// ---+---+---+  --->  ---+---+---+
// EUH| 1 | 0 |        EUH| 1 | 0 |
TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfEasyUnlockDisableUnsuccessfulThenScheduleRetry) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);
  InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult::kInternalServerError);

  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);

  VerifyDeviceInPrefs(test_devices()[0]);
  EXPECT_TRUE(mock_timer()->IsRunning());

  mock_timer()->Fire();

  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);
}

TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfNoDisablePendingThenConstructorDoesNothing) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 base::nullopt /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);
}

TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDisablePendingButIsNotCurrentEasyUnlockHostThenClearPref) {
  InitializeTest(test_devices()[0] /* initial_device_in_prefs */,
                 test_devices()[1] /* initial_better_together_host */,
                 test_devices()[1] /* initial_easy_unlock_host */);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);
}

TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDisablePendingButIsCurrentBetterTogetherHostThenClearPref) {
  InitializeTest(test_devices()[0] /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);
}

// Simulate:
//   - Disable BETTER_TOGETHER_HOST on device 0
//   - GrandfatheredEasyUnlockHostDisabler tries to disable EASY_UNLOCK_HOST on
//     device 0 but fails
//   - Timer is running while we wait to retry
//   - Re-enable BETTER_TOGETHER_HOST on device 0
TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfHostChangesWhileRetryTimerIsRunningThenCancelTimerAndClearPref) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);

  SetHost(base::nullopt, multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);
  InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult::kInternalServerError);

  EXPECT_TRUE(mock_timer()->IsRunning());

  SetHost(test_devices()[0], multidevice::SoftwareFeature::kBetterTogetherHost);

  VerifyEasyUnlockHostDisableRequest(0 /* expected_queue_size */,
                                     base::nullopt /* expected_host */);

  EXPECT_FALSE(mock_timer()->IsRunning());
  VerifyDeviceInPrefs(base::nullopt /* expected_device */);
}

// Simulate:
//   - Set device 0 as host
//   - Disable host
//   - Set device 1 as host
//   - Disable host
//   - SetSoftwareFeatureState callback for device 0 is called
TEST_P(MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
       IfDifferentHostDisabledBeforeFirstCallbackThenFirstCallbackDoesNothing) {
  InitializeTest(base::nullopt /* initial_device_in_prefs */,
                 test_devices()[0] /* initial_better_together_host */,
                 test_devices()[0] /* initial_easy_unlock_host */);
  SetHost(base::nullopt, multidevice::SoftwareFeature::kBetterTogetherHost);
  VerifyEasyUnlockHostDisableRequest(1 /* expected_queue_size */,
                                     test_devices()[0]);

  SetHost(test_devices()[1], multidevice::SoftwareFeature::kBetterTogetherHost);
  SetHost(test_devices()[1], multidevice::SoftwareFeature::kSmartLockHost);
  SetHost(base::nullopt, multidevice::SoftwareFeature::kBetterTogetherHost);
  VerifyEasyUnlockHostDisableRequest(2 /* expected_queue_size */,
                                     test_devices()[1]);
  VerifyDeviceInPrefs(test_devices()[1]);

  InvokePendingEasyUnlockHostDisableRequestCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess);
  VerifyDeviceInPrefs(test_devices()[1]);
}
// Runs tests for the following scenarios.
//   - Use v1 DeviceSync and host does not have an Instance ID.
//   - Use v1 DeviceSync and host has an Instance ID.
//   - Do not use v1 DeviceSync and host has an Instance ID.
// TODO(https://crbug.com/1019206): Remove when v1 DeviceSync is disabled,
// when all devices should have an Instance ID.
INSTANTIATE_TEST_SUITE_P(
    All,
    MultiDeviceSetupGrandfatheredEasyUnlockHostDisablerTest,
    ::testing::Values(TestType::kYesV1NoInstanceId,
                      TestType::kYesV1YesInstanceId,
                      TestType::kNoV1YesInstanceId));

}  // namespace multidevice_setup

}  // namespace chromeos
