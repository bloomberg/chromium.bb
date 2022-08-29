// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"

#include <string>

#include "ash/components/tpm/install_attributes.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Get a machine flag from |statistics_provider|, returning the given
// |default_value| if not present.
bool GetMachineFlag(chromeos::system::StatisticsProvider* statistics_provider,
                    const std::string& key,
                    bool default_value) {
  bool value = default_value;
  if (!statistics_provider->GetMachineFlag(key, &value))
    return default_value;

  return value;
}

std::string GetString(const base::Value& dict, base::StringPiece key) {
  DCHECK(dict.is_dict());
  const std::string* value = dict.FindStringKey(key);
  return value ? *value : std::string();
}

}  // namespace

namespace policy {

EnrollmentConfig::EnrollmentConfig() = default;
EnrollmentConfig::EnrollmentConfig(const EnrollmentConfig& other) = default;
EnrollmentConfig::~EnrollmentConfig() = default;

// static
EnrollmentConfig EnrollmentConfig::GetPrescribedEnrollmentConfig() {
  return GetPrescribedEnrollmentConfig(
      *g_browser_process->local_state(), *ash::InstallAttributes::Get(),
      chromeos::system::StatisticsProvider::GetInstance());
}

// static
EnrollmentConfig EnrollmentConfig::GetPrescribedEnrollmentConfig(
    const PrefService& local_state,
    const ash::InstallAttributes& install_attributes,
    chromeos::system::StatisticsProvider* statistics_provider) {
  DCHECK(statistics_provider);

  EnrollmentConfig config;

  // Authentication through the attestation mechanism is controlled by a
  // command line switch that either enables it or forces it (meaning that
  // interactive authentication is disabled).
  switch (DeviceCloudPolicyManagerAsh::GetZeroTouchEnrollmentMode()) {
    case ZeroTouchEnrollmentMode::DISABLED:
      // Only use interactive authentication.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE;
      break;

    case ZeroTouchEnrollmentMode::ENABLED:
      // Use the best mechanism, which may include attestation if available.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
      break;

    case ZeroTouchEnrollmentMode::FORCED:
    case ZeroTouchEnrollmentMode::HANDS_OFF:
      // Hands-off implies the same authentication method as Forced.

      // Only use attestation to authenticate since zero-touch is forced.
      config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
      break;
  }

  // If OOBE is done and we are not enrolled, make sure we only try interactive
  // enrollment.
  const bool oobe_complete = local_state.GetBoolean(ash::prefs::kOobeComplete);
  if (oobe_complete &&
      config.auth_mechanism == EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE)
    config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE;
  // If OOBE is done and we are enrolled, check for need to recover enrollment.
  // Enrollment recovery is not implemented for Active Directory.
  if (oobe_complete && install_attributes.IsCloudManaged()) {
    // Regardless what mode is applicable, the enrollment domain is fixed.
    config.management_domain = install_attributes.GetDomain();

    // Enrollment has completed previously and installation-time attributes
    // are in place. Enrollment recovery is required when the server
    // registration gets lost.
    if (local_state.GetBoolean(prefs::kEnrollmentRecoveryRequired)) {
      LOG(WARNING) << "Enrollment recovery required according to pref.";
      if (statistics_provider->GetEnterpriseMachineID().empty())
        LOG(WARNING) << "Postponing recovery because machine id is missing.";
      else
        config.mode = EnrollmentConfig::MODE_RECOVERY;
    }

    return config;
  }

  // OOBE is still running, or it is complete but the device hasn't been
  // enrolled yet. In either case, enrollment should take place if there's a
  // signal present that indicates the device should enroll.

  // Gather enrollment signals from various sources.
  const base::Value* device_state =
      local_state.GetDictionary(prefs::kServerBackedDeviceState);
  std::string device_state_mode;
  std::string device_state_management_domain;
  bool is_license_packaged_with_device = false;
  std::string license_type;

  if (device_state) {
    device_state_mode = GetString(*device_state, kDeviceStateMode);
    device_state_management_domain =
        GetString(*device_state, kDeviceStateManagementDomain);
    is_license_packaged_with_device =
        device_state->FindBoolPath(kDeviceStatePackagedLicense).value_or(false);
    license_type = GetString(*device_state, kDeviceStateLicenseType);
  }

  config.is_license_packaged_with_device = is_license_packaged_with_device;

  if (license_type == kDeviceStateLicenseTypeEnterprise) {
    config.license_type = LicenseType::kEnterprise;
  } else if (license_type == kDeviceStateLicenseTypeEducation) {
    config.license_type = LicenseType::kEducation;
  } else if (license_type == kDeviceStateLicenseTypeTerminal) {
    config.license_type = LicenseType::kTerminal;
  } else {
    config.license_type = LicenseType::kNone;
  }

  const bool pref_enrollment_auto_start_present =
      local_state.HasPrefPath(prefs::kDeviceEnrollmentAutoStart);
  const bool pref_enrollment_auto_start =
      local_state.GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  const bool pref_enrollment_can_exit_present =
      local_state.HasPrefPath(prefs::kDeviceEnrollmentCanExit);
  const bool pref_enrollment_can_exit =
      local_state.GetBoolean(prefs::kDeviceEnrollmentCanExit);

  const bool oem_is_managed = GetMachineFlag(
      statistics_provider, chromeos::system::kOemIsEnterpriseManagedKey, false);
  const bool oem_can_exit_enrollment = GetMachineFlag(
      statistics_provider, chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
      true);

  // Decide enrollment mode. Give precedence to forced variants.
  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentEnforced) {
    config.mode = EnrollmentConfig::MODE_SERVER_FORCED;
    config.management_domain = device_state_management_domain;
  } else if (device_state_mode == kDeviceStateInitialModeEnrollmentEnforced) {
    config.mode = EnrollmentConfig::MODE_INITIAL_SERVER_FORCED;
    config.management_domain = device_state_management_domain;
  } else if (device_state_mode ==
             kDeviceStateRestoreModeReEnrollmentZeroTouch) {
    config.mode = EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED;
    config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    config.management_domain = device_state_management_domain;
  } else if (device_state_mode == kDeviceStateInitialModeEnrollmentZeroTouch) {
    config.mode = EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED;
    config.auth_mechanism = EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    config.management_domain = device_state_management_domain;
  } else if (pref_enrollment_auto_start_present && pref_enrollment_auto_start &&
             pref_enrollment_can_exit_present && !pref_enrollment_can_exit) {
    config.mode = EnrollmentConfig::MODE_LOCAL_FORCED;
  } else if (oem_is_managed && !oem_can_exit_enrollment) {
    config.mode = EnrollmentConfig::MODE_LOCAL_FORCED;
  } else if (oobe_complete) {
    // If OOBE is complete, don't return advertised modes as there's currently
    // no way to make sure advertised enrollment only gets shown once.
    config.mode = EnrollmentConfig::MODE_NONE;
  } else if (device_state_mode ==
             kDeviceStateRestoreModeReEnrollmentRequested) {
    config.mode = EnrollmentConfig::MODE_SERVER_ADVERTISED;
    config.management_domain = device_state_management_domain;
  } else if (pref_enrollment_auto_start_present && pref_enrollment_auto_start) {
    config.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED;
  } else if (oem_is_managed) {
    config.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED;
  }

  return config;
}

}  // namespace policy
