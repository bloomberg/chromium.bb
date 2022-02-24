// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_policy_handler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/policy_util.h"

namespace chromeos {

namespace {

const int kInstallRetryLimit = 3;

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors to ignore.
    5 * 60 * 1000,   // Initial delay of 5 minutes in ms.
    2.0,             // Factor by which the waiting time will be multiplied.
    0,               // Fuzzing percentage.
    60 * 60 * 1000,  // Maximum delay of 1 hour in ms.
    -1,              // Never discard the entry.
    true,            // Use initial delay.
};

// Timeout waiting for EUICC to become available in Hermes.
constexpr base::TimeDelta kEuiccWaitTime = base::Minutes(3);
// Timeout waiting for profile list to be refreshed.
constexpr base::TimeDelta kProfileRefreshWaitTime = base::Seconds(90);

}  // namespace

CellularPolicyHandler::InstallPolicyESimRequest::InstallPolicyESimRequest(
    const std::string& smdp_address,
    const base::Value& onc_config)
    : smdp_address(smdp_address),
      onc_config(onc_config.Clone()),
      retry_backoff(&kRetryBackoffPolicy) {}

CellularPolicyHandler::InstallPolicyESimRequest::~InstallPolicyESimRequest() =
    default;

CellularPolicyHandler::CellularPolicyHandler() = default;

CellularPolicyHandler::~CellularPolicyHandler() = default;

void CellularPolicyHandler::Init(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    CellularESimInstaller* cellular_esim_installer,
    NetworkProfileHandler* network_profile_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  cellular_esim_installer_ = cellular_esim_installer;
  network_profile_handler_ = network_profile_handler;
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;

  hermes_observation_.Observe(HermesManagerClient::Get());
  cellular_esim_profile_handler_observation_.Observe(
      cellular_esim_profile_handler);
}

void CellularPolicyHandler::InstallESim(const std::string& smdp_address,
                                        const base::Value& onc_config) {
  PushRequestAndProcess(
      std::make_unique<InstallPolicyESimRequest>(smdp_address, onc_config));
}

void CellularPolicyHandler::OnAvailableEuiccListChanged() {
  ResumeInstallIfNeeded();
}

void CellularPolicyHandler::OnESimProfileListUpdated() {
  ResumeInstallIfNeeded();
}

void CellularPolicyHandler::ResumeInstallIfNeeded() {
  if (!is_installing_ || !wait_timer_.IsRunning()) {
    return;
  }
  wait_timer_.Stop();
  AttemptInstallESim();
}

void CellularPolicyHandler::ProcessRequests() {
  if (remaining_install_requests_.empty())
    return;

  // Another install request is already underway; wait until it has completed
  // before starting a new request.
  if (is_installing_)
    return;

  is_installing_ = true;
  NET_LOG(EVENT)
      << "Starting installing policy eSIM profile with SMDP address: "
      << GetCurrentSmdpAddress();
  AttemptInstallESim();
}

void CellularPolicyHandler::AttemptInstallESim() {
  DCHECK(is_installing_);

  absl::optional<dbus::ObjectPath> euicc_path = GetCurrentEuiccPath();
  if (!euicc_path) {
    // Hermes may not be ready and available euicc list is empty. Wait for
    // AvailableEuiccListChanged notification to continue with installation.
    NET_LOG(EVENT) << "No EUICC found when attempting to install eSIM profile "
                      "for SMDP address: "
                   << GetCurrentSmdpAddress() << ". Waiting for EUICC.";
    wait_timer_.Start(FROM_HERE, kEuiccWaitTime,
                      base::BindOnce(&CellularPolicyHandler::OnWaitTimeout,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!cellular_esim_profile_handler_->HasRefreshedProfilesForEuicc(
          *euicc_path)) {
    // Profile list for current euicc may not have been refreshed. Wait for
    // ProfileListChanged notification to continue with installation.
    NET_LOG(EVENT) << "Profile list not refreshed for EUICC when attempting to "
                      "install eSIM profile for SMDP address: "
                   << GetCurrentSmdpAddress()
                   << ". Waiting for profile list change.";
    wait_timer_.Start(FROM_HERE, kProfileRefreshWaitTime,
                      base::BindOnce(&CellularPolicyHandler::OnWaitTimeout,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NET_LOG(EVENT) << "Attempt setup policy eSIM profile with SMDP address: "
                 << GetCurrentSmdpAddress()
                 << " on euicc path: " << euicc_path->value();
  SetupESim(*euicc_path);
}

void CellularPolicyHandler::SetupESim(const dbus::ObjectPath& euicc_path) {
  base::Value new_shill_properties = GetNewShillProperties();
  absl::optional<dbus::ObjectPath> profile_path =
      FindExistingMatchingESimProfile();
  if (profile_path) {
    NET_LOG(EVENT)
        << "Setting up network for existing eSIM profile with properties: "
        << new_shill_properties;
    cellular_esim_installer_->ConfigureESimService(
        std::move(new_shill_properties), euicc_path, *profile_path,
        base::BindOnce(&CellularPolicyHandler::OnConfigureESimService,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NET_LOG(EVENT) << "Install policy eSIM profile with properties: "
                 << new_shill_properties;
  // Remote provisioning of eSIM profiles via SMDP address in policy does not
  // require confirmation code.
  cellular_esim_installer_->InstallProfileFromActivationCode(
      GetCurrentSmdpAddress(), /*confirmation_code=*/std::string(), euicc_path,
      std::move(new_shill_properties),
      base::BindOnce(
          &CellularPolicyHandler::OnESimProfileInstallAttemptComplete,
          weak_ptr_factory_.GetWeakPtr()),
      remaining_install_requests_.front()->retry_backoff.failure_count() == 0);
}

base::Value CellularPolicyHandler::GetNewShillProperties() {
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(
          /*userhash=*/std::string());
  const std::string* guid =
      remaining_install_requests_.front()->onc_config.FindStringKey(
          ::onc::network_config::kGUID);
  DCHECK(guid);

  return policy_util::CreateShillConfiguration(
      *profile, *guid, /*global_policy=*/nullptr,
      &(remaining_install_requests_.front()->onc_config),
      /*user_settings=*/nullptr);
}

const std::string& CellularPolicyHandler::GetCurrentSmdpAddress() const {
  DCHECK(is_installing_);

  return remaining_install_requests_.front()->smdp_address;
}

void CellularPolicyHandler::OnConfigureESimService(
    absl::optional<dbus::ObjectPath> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  if (service_path) {
    NET_LOG(EVENT)
        << "Successfully configured service for existing eSIM profile";
    current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
    ProcessRequests();
    return;
  }

  ScheduleRetry(std::move(current_request));
  ProcessRequests();
}

void CellularPolicyHandler::OnESimProfileInstallAttemptComplete(
    HermesResponseStatus hermes_status,
    absl::optional<dbus::ObjectPath> profile_path,
    absl::optional<std::string> service_path) {
  DCHECK(is_installing_);

  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  if (hermes_status == HermesResponseStatus::kSuccess) {
    NET_LOG(EVENT) << "Successfully installed eSIM profile with SMDP address: "
                   << current_request->smdp_address;
    current_request->retry_backoff.InformOfRequest(/*succeeded=*/true);
    managed_network_configuration_handler_->NotifyPolicyAppliedToNetwork(
        *service_path);

    ProcessRequests();
    return;
  }

  ScheduleRetry(std::move(current_request));
  ProcessRequests();
}

void CellularPolicyHandler::ScheduleRetry(
    std::unique_ptr<InstallPolicyESimRequest> request) {
  if (request->retry_backoff.failure_count() >= kInstallRetryLimit) {
    NET_LOG(ERROR) << "Install policy eSIM profile with SMDP address: "
                   << request->smdp_address << " failed " << kInstallRetryLimit
                   << " times.";
    ProcessRequests();
    return;
  }

  request->retry_backoff.InformOfRequest(/*succeeded=*/false);
  base::TimeDelta retry_delay = request->retry_backoff.GetTimeUntilRelease();
  NET_LOG(ERROR) << "Install policy eSIM profile failed. Retrying in "
                 << retry_delay;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CellularPolicyHandler::PushRequestAndProcess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)),
      retry_delay);
}

void CellularPolicyHandler::PushRequestAndProcess(
    std::unique_ptr<InstallPolicyESimRequest> request) {
  remaining_install_requests_.push_back(std::move(request));
  ProcessRequests();
}

void CellularPolicyHandler::PopRequest() {
  remaining_install_requests_.pop_front();
  is_installing_ = false;
  if (remaining_install_requests_.empty()) {
    const NetworkProfile* profile =
        network_profile_handler_->GetProfileForUserhash(
            /*userhash=*/std::string());
    DCHECK(profile);

    managed_network_configuration_handler_->OnCellularPoliciesApplied(*profile);
  }
}

absl::optional<dbus::ObjectPath>
CellularPolicyHandler::FindExistingMatchingESimProfile() {
  const base::Value* cellular_properties =
      remaining_install_requests_.front()->onc_config.FindDictKey(
          ::onc::network_config::kCellular);
  if (!cellular_properties) {
    NET_LOG(ERROR) << "Missing Cellular properties in ONC config.";
    return absl::nullopt;
  }
  const std::string* iccid =
      cellular_properties->FindStringKey(::onc::cellular::kICCID);
  if (!iccid) {
    return absl::nullopt;
  }
  for (CellularESimProfile esim_profile :
       cellular_esim_profile_handler_->GetESimProfiles()) {
    if (esim_profile.iccid() == *iccid) {
      return esim_profile.path();
    }
  }
  return absl::nullopt;
}

void CellularPolicyHandler::OnWaitTimeout() {
  NET_LOG(ERROR) << "Install request for SMDP address: "
                 << GetCurrentSmdpAddress()
                 << ". Timed out waiting for EUICC or profile list.";
  auto current_request = std::move(remaining_install_requests_.front());
  PopRequest();
  ScheduleRetry(std::move(current_request));
  ProcessRequests();
}

}  // namespace chromeos
