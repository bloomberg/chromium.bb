// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

// Fetches a machine statistic value from StatisticsProvider, returns an empty
// string on failure.
std::string GetMachineStatistic(const std::string& key) {
  std::string value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(key, &value))
    return std::string();

  return value;
}

// Gets a machine flag from StatisticsProvider, returns the given
// |default_value| if not present.
bool GetMachineFlag(const std::string& key, bool default_value) {
  bool value = default_value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineFlag(key, &value))
    return default_value;

  return value;
}

}  // namespace

// static
const char EnrollmentRequisitionManager::kNoRequisition[] = "none";
const char EnrollmentRequisitionManager::kRemoraRequisition[] = "remora";
const char EnrollmentRequisitionManager::kSharkRequisition[] = "shark";
const char EnrollmentRequisitionManager::kRialtoRequisition[] = "rialto";

// static
void EnrollmentRequisitionManager::Initialize() {
  // OEM statistics are only loaded when OOBE is not completed.
  if (ash::StartupUtils::IsOobeCompleted())
    return;

  // Demo requisition may have been set in a prior enrollment attempt that was
  // interrupted.
  ash::DemoSetupController::ClearDemoRequisition();
  auto* local_state = g_browser_process->local_state();
  const PrefService::Preference* pref =
      local_state->FindPreference(prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    std::string requisition =
        GetMachineStatistic(chromeos::system::kOemDeviceRequisitionKey);

    if (!requisition.empty()) {
      local_state->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
      if (requisition == kRemoraRequisition ||
          requisition == kSharkRequisition ||
          requisition == kRialtoRequisition) {
        SetDeviceEnrollmentAutoStart();
      } else {
        local_state->SetBoolean(
            prefs::kDeviceEnrollmentAutoStart,
            GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey,
                           false));
        local_state->SetBoolean(
            prefs::kDeviceEnrollmentCanExit,
            GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                           false));
      }
    }
  }
}

// static
std::string EnrollmentRequisitionManager::GetDeviceRequisition() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDeviceEnrollmentRequisition);
  std::string requisition;
  if (!pref->IsDefaultValue() && pref->GetValue()->is_string())
    requisition = pref->GetValue()->GetString();

  if (requisition == kNoRequisition)
    requisition.clear();

  return requisition;
}

// static
void EnrollmentRequisitionManager::SetDeviceRequisition(
    const std::string& requisition) {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "SetDeviceRequisition " << requisition;

  auto* local_state = g_browser_process->local_state();
  if (requisition.empty()) {
    local_state->ClearPref(prefs::kDeviceEnrollmentRequisition);
    local_state->ClearPref(prefs::kDeviceEnrollmentAutoStart);
    local_state->ClearPref(prefs::kDeviceEnrollmentCanExit);
  } else {
    local_state->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
    if (requisition == kNoRequisition) {
      local_state->ClearPref(prefs::kDeviceEnrollmentAutoStart);
      local_state->ClearPref(prefs::kDeviceEnrollmentCanExit);
    } else {
      SetDeviceEnrollmentAutoStart();
    }
  }
}

// static
bool EnrollmentRequisitionManager::IsRemoraRequisition() {
  return GetDeviceRequisition() == kRemoraRequisition;
}

// static
bool EnrollmentRequisitionManager::IsSharkRequisition() {
  return GetDeviceRequisition() == kSharkRequisition;
}

// static
std::string EnrollmentRequisitionManager::GetSubOrganization() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDeviceEnrollmentSubOrganization);
  if (!pref->IsDefaultValue() && pref->GetValue()->is_string())
    return pref->GetValue()->GetString();
  return std::string();
}

// static
void EnrollmentRequisitionManager::SetSubOrganization(
    const std::string& sub_organization) {
  if (sub_organization.empty())
    g_browser_process->local_state()->ClearPref(
        prefs::kDeviceEnrollmentSubOrganization);
  else
    g_browser_process->local_state()->SetString(
        prefs::kDeviceEnrollmentSubOrganization, sub_organization);
}

// static
void EnrollmentRequisitionManager::SetDeviceEnrollmentAutoStart() {
  g_browser_process->local_state()->SetBoolean(
      prefs::kDeviceEnrollmentAutoStart, true);
  g_browser_process->local_state()->SetBoolean(prefs::kDeviceEnrollmentCanExit,
                                               false);
}

// static
void EnrollmentRequisitionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterStringPref(prefs::kDeviceEnrollmentSubOrganization,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentCanExit, true);
}

}  // namespace policy
