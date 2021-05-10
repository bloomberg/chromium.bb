// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/multidevice_setup_state_updater.h"

#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

class MultideviceSetupStateUpdaterTest : public testing::Test {
 protected:
  MultideviceSetupStateUpdaterTest() = default;
  ~MultideviceSetupStateUpdaterTest() override = default;

  MultideviceSetupStateUpdaterTest(const MultideviceSetupStateUpdaterTest&) =
      delete;
  MultideviceSetupStateUpdaterTest& operator=(
      const MultideviceSetupStateUpdaterTest&) = delete;

  // testing::Test:
  void SetUp() override {
    MultideviceSetupStateUpdater::RegisterPrefs(pref_service_.registry());
    multidevice_setup::RegisterFeaturePrefs(pref_service_.registry());

    // Set the host status and feature state to realistic default values used
    // during start-up.
    SetFeatureState(Feature::kPhoneHub,
                    FeatureState::kUnavailableNoVerifiedHost);
    SetHostStatus(HostStatus::kNoEligibleHosts);
  }

  void CreateUpdater() {
    updater_ = std::make_unique<MultideviceSetupStateUpdater>(
        &pref_service_, &fake_multidevice_setup_client_,
        &fake_notification_access_manager_);
  }

  void DestroyUpdater() { updater_.reset(); }

  void SetNotificationAccess(bool enabled) {
    fake_notification_access_manager_.SetAccessStatusInternal(
        enabled
            ? NotificationAccessManager::AccessStatus::kAccessGranted
            : NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  }

  void SetFeatureState(Feature feature, FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(feature, feature_state);
  }

  void SetHostStatus(HostStatus host_status) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(host_status, base::nullopt /* host_device */));
  }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return &fake_multidevice_setup_client_;
  }

 private:
  std::unique_ptr<MultideviceSetupStateUpdater> updater_;

  TestingPrefServiceSimple pref_service_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  FakeNotificationAccessManager fake_notification_access_manager_;
};

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub) {
  CreateUpdater();

  // Test that there is a call to enable kPhoneHub--if it is currently
  // disabled--when host status goes from
  // kHostSetLocallyButWaitingForBackendConfirmation to kHostVerified.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub_SetButNotVerified) {
  CreateUpdater();

  // Test that there is a call to enable kPhoneHub when host status goes from
  // kHostSetLocallyButWaitingForBackendConfirmation to
  // kHostSetButNotYetVerified, then finally to kHostVerified, when the feature
  // is currently disabled.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostSetButNotYetVerified);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       EnablePhoneHub_WaitForDisabledStateBeforeEnabling) {
  CreateUpdater();

  // After the host is verified, ensure that we wait until the feature state is
  // "disabled" before enabling the feature. We don't want to go from
  // kNotSupportedByPhone to enabled, for instance.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kNotSupportedByPhone);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       EnablePhoneHub_DisabledStateSetBeforeVerification) {
  CreateUpdater();

  // Much like EnablePhoneHub_WaitForDisabledStateBeforeEnabling, but here we
  // test that the order of the feature being set to disabled and the host being
  // verified does not matter.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kNotSupportedByPhone);
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetHostStatus(HostStatus::kHostVerified);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       EnablePhoneHub_ReenableAfterMultideviceSetup) {
  CreateUpdater();

  // The user has a verified host phone, but chose to disable the Phone Hub
  // toggle in Settings.
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  // The user disconnects the phone from the multi-device suite.
  SetHostStatus(HostStatus::kEligibleHostExistsButNoHostSet);

  // The user goes through multi-device setup again.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetHostStatus(HostStatus::kHostVerified);

  // The Phone Hub feature is automatically re-enabled.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, EnablePhoneHub_PersistIntentToEnable) {
  CreateUpdater();

  // Indicate intent to enable Phone Hub after host verification.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);

  // Simulate the user logging out and back in, for instance. And even though
  // some transient default values are set for the host status and feature
  // state, we should preserve the intent to enable Phone Hub.
  DestroyUpdater();
  SetFeatureState(Feature::kPhoneHub, FeatureState::kUnavailableNoVerifiedHost);
  SetHostStatus(HostStatus::kNoEligibleHosts);
  CreateUpdater();

  // The host status and feature state update to expected values.
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  // The Phone Hub feature is expected to be enabled.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(
    MultideviceSetupStateUpdaterTest,
    EnablePhoneHub_PersistIntentToEnable_HandleTransientHostOrFeatureStates) {
  CreateUpdater();

  // Indicate intent to enable Phone Hub after host verification.
  SetHostStatus(HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);

  // Simulate the user logging out and back in.
  DestroyUpdater();
  CreateUpdater();

  // Make sure to ignore transient updates after start-up. In other words,
  // maintain our intent to enable Phone Hub after verification.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kUnavailableNoVerifiedHost);
  SetHostStatus(HostStatus::kNoEligibleHosts);

  // The host status and feature state update to expected values.
  SetHostStatus(HostStatus::kHostVerified);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  // The Phone Hub feature is expected to be enabled.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHub,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, RevokePhoneHubNotificationsAccess) {
  SetNotificationAccess(true);
  CreateUpdater();

  // Test that there is a call to disable kPhoneHubNotifications when
  // notification access has been revoked.
  SetNotificationAccess(false);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubNotifications,
      /*expected_enabled=*/false, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest, InitiallyEnablePhoneHubNotifications) {
  SetNotificationAccess(false);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  CreateUpdater();

  // If the notifications feature has not been explicitly set yet, enable it
  // when Phone Hub is enabled and access has been granted.
  SetNotificationAccess(true);
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kPhoneHubNotifications,
      /*expected_enabled=*/true, /*expected_auth_token=*/base::nullopt,
      /*success=*/true);
}

TEST_F(MultideviceSetupStateUpdaterTest,
       InitiallyEnablePhoneHubNotifications_OnlyEnableFromDefaultState) {
  SetNotificationAccess(false);
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);

  // Explicitly disable Phone Hub notifications.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);

  CreateUpdater();

  // We take no action after access is granted because the Phone Hub
  // notifications feature state was already explicitly set; we respect the
  // user's choice.
  SetNotificationAccess(true);
  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

}  // namespace phonehub
}  // namespace chromeos
