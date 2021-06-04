// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/multidevice_setup_state_updater.h"

#include "base/callback_helpers.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "chromeos/components/phonehub/util/histogram_util.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace phonehub {
namespace {
using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;
}  // namespace

// static
void MultideviceSetupStateUpdater::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kIsAwaitingVerifiedHost, false);
}

MultideviceSetupStateUpdater::MultideviceSetupStateUpdater(
    PrefService* pref_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    NotificationAccessManager* notification_access_manager)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      notification_access_manager_(notification_access_manager) {
  multidevice_setup_client_->AddObserver(this);
  notification_access_manager_->AddObserver(this);
}

MultideviceSetupStateUpdater::~MultideviceSetupStateUpdater() {
  multidevice_setup_client_->RemoveObserver(this);
  notification_access_manager_->RemoveObserver(this);
}

void MultideviceSetupStateUpdater::OnNotificationAccessChanged() {
  switch (notification_access_manager_->GetAccessStatus()) {
    case NotificationAccessManager::AccessStatus::kAccessGranted:
      if (IsWaitingForAccessToInitiallyEnableNotifications()) {
        PA_LOG(INFO) << "Enabling PhoneHubNotifications for the first time now "
                     << "that access has been granted by the phone.";
        multidevice_setup_client_->SetFeatureEnabledState(
            Feature::kPhoneHubNotifications, /*enabled=*/true,
            /*auth_token=*/absl::nullopt, base::DoNothing());
      }
      break;

    case NotificationAccessManager::AccessStatus::kAvailableButNotGranted:
      FALLTHROUGH;
    case NotificationAccessManager::AccessStatus::kProhibited:
      // Disable kPhoneHubNotifications if notification access has been revoked
      // by the phone.
      PA_LOG(INFO) << "Disabling PhoneHubNotifications feature.";
      multidevice_setup_client_->SetFeatureEnabledState(
          Feature::kPhoneHubNotifications, /*enabled=*/false,
          /*auth_token=*/absl::nullopt, base::DoNothing());
      break;
  }
}

void MultideviceSetupStateUpdater::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  EnablePhoneHubIfAwaitingVerifiedHost();
}

void MultideviceSetupStateUpdater::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_state_map) {
  EnablePhoneHubIfAwaitingVerifiedHost();
}

bool MultideviceSetupStateUpdater::
    IsWaitingForAccessToInitiallyEnableNotifications() const {
  // If the Phone Hub notifications feature has never been explicitly set, we
  // should enable it after
  //   1. the top-level Phone Hub feature is enabled, and
  //   2. the phone has granted access.
  // We do *not* want disrupt the feature state if it was already explicitly set
  // by the user.
  return multidevice_setup::IsDefaultFeatureEnabledValue(
             Feature::kPhoneHubNotifications, pref_service_) &&
         multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub) ==
             FeatureState::kEnabledByUser;
}

void MultideviceSetupStateUpdater::EnablePhoneHubIfAwaitingVerifiedHost() {
  bool is_awaiting_verified_host =
      pref_service_->GetBoolean(prefs::kIsAwaitingVerifiedHost);
  const HostStatus host_status =
      multidevice_setup_client_->GetHostStatus().first;
  const FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub);

  // Enable the PhoneHub feature if the phone is verified and there was an
  // intent to enable the feature. We also ensure that the feature is currently
  // disabled and not in state like kNotSupportedByPhone or kProhibitedByPolicy.
  if (is_awaiting_verified_host && host_status == HostStatus::kHostVerified &&
      feature_state == FeatureState::kDisabledByUser) {
    multidevice_setup_client_->SetFeatureEnabledState(
        Feature::kPhoneHub, /*enabled=*/true, /*auth_token=*/absl::nullopt,
        base::DoNothing());
    util::LogFeatureOptInEntryPoint(util::OptInEntryPoint::kSetupFlow);
  }

  UpdateIsAwaitingVerifiedHost();
}

void MultideviceSetupStateUpdater::UpdateIsAwaitingVerifiedHost() {
  // Wait to enable Phone Hub until after host phone is verified. The intent to
  // enable Phone Hub must be persisted in the event that this class is
  // destroyed before the phone is verified.
  const HostStatus host_status =
      multidevice_setup_client_->GetHostStatus().first;
  if (host_status ==
      HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    pref_service_->SetBoolean(prefs::kIsAwaitingVerifiedHost, true);
    return;
  }

  // The intent to enable Phone Hub after host verification was fulfilled.
  // Note: We don't want to reset the pref if, say, the host status is
  // kNoEligibleHosts; that might just be a transient state seen during
  // start-up, for instance. It is true that we don't want to enable Phone Hub
  // if the user explicitly disabled it in settings, however, that can only
  // occur after the host becomes verified and we first enable Phone Hub.
  const bool is_awaiting_verified_host =
      pref_service_->GetBoolean(prefs::kIsAwaitingVerifiedHost);
  const FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(Feature::kPhoneHub);
  if (is_awaiting_verified_host && host_status == HostStatus::kHostVerified &&
      feature_state == FeatureState::kEnabledByUser) {
    pref_service_->SetBoolean(prefs::kIsAwaitingVerifiedHost, false);
    return;
  }
}

}  // namespace phonehub
}  // namespace chromeos
