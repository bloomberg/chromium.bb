// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_installer.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/hermes_metrics_util.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace {

// Measures the time from which this function is called to when |callback|
// is expected to run. The measured time difference should capture the time it
// took for a profile to be fully downloaded from a provided activation code.
CellularESimInstaller::InstallProfileFromActivationCodeCallback
CreateTimedInstallProfileCallback(
    CellularESimInstaller::InstallProfileFromActivationCodeCallback callback) {
  return base::BindOnce(
      [](CellularESimInstaller::InstallProfileFromActivationCodeCallback
             callback,
         base::Time installation_start_time, HermesResponseStatus result,
         absl::optional<dbus::ObjectPath> esim_profile_path,
         absl::optional<std::string> service_path) -> void {
        std::move(callback).Run(result, esim_profile_path, service_path);
        if (result != HermesResponseStatus::kSuccess)
          return;
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency",
            base::Time::Now() - installation_start_time);
      },
      std::move(callback), base::Time::Now());
}

void AppendRequiredCellularProperties(const dbus::ObjectPath& euicc_path,
                                      const dbus::ObjectPath& profile_path,
                                      const NetworkProfile* profile,
                                      base::Value* shill_properties_to_update) {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  shill_properties_to_update->SetStringKey(shill::kTypeProperty,
                                           shill::kTypeCellular);
  shill_properties_to_update->SetStringKey(shill::kProfileProperty,
                                           profile->path);
  // Insert ICCID and EID to the shill properties.
  shill_properties_to_update->SetStringKey(shill::kIccidProperty,
                                           profile_properties->iccid().value());
  shill_properties_to_update->SetStringKey(shill::kEidProperty,
                                           euicc_properties->eid().value());
  // Use ICCID as GUID if no GUID found in the given properties.
  if (!shill_properties_to_update->FindStringKey(shill::kGuidProperty))
    shill_properties_to_update->SetStringKey(
        shill::kGuidProperty, profile_properties->iccid().value());
}

bool IsManagedNetwork(const base::Value& new_shill_properties) {
  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(
          base::Value::AsDictionaryValue(new_shill_properties));
  return ui_data && (ui_data->onc_source() == ::onc::ONC_SOURCE_DEVICE_POLICY ||
                     ui_data->onc_source() == ::onc::ONC_SOURCE_USER_POLICY);
}

}  // namespace

// static
void CellularESimInstaller::RecordInstallESimProfileResult(
    InstallESimProfileResult result,
    bool is_managed) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.InstallationResult",
                                result);

  if (is_managed) {
    base::UmaHistogramEnumeration(
        "Network.Cellular.ESim.Policy.ESimInstall.OperationResult", result);
    return;
  }
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.InstallViaQrCode.OperationResult", result);
}

CellularESimInstaller::CellularESimInstaller() = default;

CellularESimInstaller::~CellularESimInstaller() = default;

void CellularESimInstaller::Init(
    CellularConnectionHandler* cellular_connection_handler,
    CellularInhibitor* cellular_inhibitor,
    NetworkConnectionHandler* network_connection_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_connection_handler_ = cellular_connection_handler;
  cellular_inhibitor_ = cellular_inhibitor;
  network_connection_handler_ = network_connection_handler;
  network_profile_handler_ = network_profile_handler;
  network_state_handler_ = network_state_handler;
}

void CellularESimInstaller::InstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    const dbus::ObjectPath& euicc_path,
    base::Value new_shill_properties,
    InstallProfileFromActivationCodeCallback callback) {
  // Try installing directly with activation code.
  // TODO(crbug.com/1186682) Add a check for activation codes that are
  // currently being installed to prevent multiple attempts for the same
  // activation code.
  NET_LOG(USER) << "Attempting installation with code " << activation_code;
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kInstallingProfile,
      base::BindOnce(
          &CellularESimInstaller::PerformInstallProfileFromActivationCode,
          weak_ptr_factory_.GetWeakPtr(), activation_code, confirmation_code,
          euicc_path, std::move(new_shill_properties),
          CreateTimedInstallProfileCallback(std::move(callback))));
}

void CellularESimInstaller::PerformInstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    const dbus::ObjectPath& euicc_path,
    base::Value new_shill_properties,
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    RecordInstallESimProfileResult(InstallESimProfileResult::kInhibitFailed,
                                   IsManagedNetwork(new_shill_properties));
    std::move(callback).Run(HermesResponseStatus::kErrorWrongState,
                            /*profile_path=*/absl::nullopt,
                            /*service_path=*/absl::nullopt);
    return;
  }

  HermesEuiccClient::Get()->InstallProfileFromActivationCode(
      euicc_path, activation_code, confirmation_code,
      base::BindOnce(&CellularESimInstaller::OnProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(inhibit_lock), euicc_path,
                     std::move(new_shill_properties)));
}

void CellularESimInstaller::OnProfileInstallResult(
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    const dbus::ObjectPath& euicc_path,
    const base::Value& new_shill_properties,
    HermesResponseStatus status,
    const dbus::ObjectPath* profile_path) {
  hermes_metrics::LogInstallViaQrCodeResult(status);

  bool is_managed = IsManagedNetwork(new_shill_properties);
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing profile status="
                   << static_cast<int>(status);
    RecordInstallESimProfileResult(
        InstallESimProfileResult::kHermesInstallFailed, is_managed);
    std::move(callback).Run(status, /*profile_path=*/absl::nullopt,
                            /*service_path=*/absl::nullopt);
    return;
  }

  RecordInstallESimProfileResult(InstallESimProfileResult::kSuccess,
                                 is_managed);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(
          /*userhash=*/std::string());
  if (!profile) {
    NET_LOG(ERROR)
        << "Error installing eSIM profile. Default profile not initialized.";
    std::move(callback).Run(HermesResponseStatus::kErrorUnknown,
                            /*profile_path=*/absl::nullopt,
                            /*service_path=*/absl::nullopt);
    return;
  }

  base::Value properties_to_set = new_shill_properties.Clone();
  AppendRequiredCellularProperties(euicc_path, *profile_path, profile,
                                   &properties_to_set);
  NET_LOG(EVENT) << "Creating shill configuration for newly installed eSim "
                    "profile with properties: "
                 << properties_to_set;

  pending_inhibit_locks_.emplace(*profile_path, std::move(inhibit_lock));
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->ConfigureServiceForProfile(
      dbus::ObjectPath(profile->path), properties_to_set,
      base::BindOnce(
          &CellularESimInstaller::OnShillConfigurationCreationSuccess,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_split.first),
          euicc_path, *profile_path),
      base::BindOnce(
          &CellularESimInstaller::OnShillConfigurationCreationFailure,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_split.second),
          euicc_path, *profile_path));
}

void CellularESimInstaller::OnShillConfigurationCreationSuccess(
    InstallProfileFromActivationCodeCallback callback,
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    const dbus::ObjectPath& service_path) {
  NET_LOG(EVENT)
      << "Successfully creating shill configuration on service path: "
      << service_path.value();

  EnableProfile(std::move(callback), euicc_path, profile_path);
}

void CellularESimInstaller::OnShillConfigurationCreationFailure(
    InstallProfileFromActivationCodeCallback callback,
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Create shill configuration failed, error:" << error_name
                 << ", message: " << error_message;

  EnableProfile(std::move(callback), euicc_path, profile_path);
}

void CellularESimInstaller::EnableProfile(
    InstallProfileFromActivationCodeCallback callback,
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path) {
  auto it = pending_inhibit_locks_.find(profile_path);
  DCHECK(it != pending_inhibit_locks_.end());

  auto callback_split = base::SplitOnceCallback(std::move(callback));
  cellular_connection_handler_
      ->PrepareNewlyInstalledCellularNetworkForConnection(
          euicc_path, profile_path, std::move(it->second),
          base::BindOnce(&CellularESimInstaller::
                             OnPrepareCellularNetworkForConnectionSuccess,
                         weak_ptr_factory_.GetWeakPtr(), profile_path,
                         std::move(callback_split.first)),
          base::BindOnce(&CellularESimInstaller::
                             OnPrepareCellularNetworkForConnectionFailure,
                         weak_ptr_factory_.GetWeakPtr(), profile_path,
                         std::move(callback_split.second)));
  pending_inhibit_locks_.erase(it);
}

void CellularESimInstaller::OnPrepareCellularNetworkForConnectionSuccess(
    const dbus::ObjectPath& profile_path,
    InstallProfileFromActivationCodeCallback callback,
    const std::string& service_path) {
  NET_LOG(EVENT) << "Successfully enabled installed profile on service path: "
                 << service_path;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state) {
    HandleNewProfileEnableFailure(std::move(callback), profile_path,
                                  service_path,
                                  NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  if (!network_state->IsConnectingOrConnected()) {
    // The connection could fail but the user will be notified of connection
    // failures separately.
    network_connection_handler_->ConnectToNetwork(
        service_path,
        /*success_callback=*/base::DoNothing(),
        /*error_callback=*/base::DoNothing(),
        /*check_error_state=*/false, ConnectCallbackMode::ON_STARTED);
  }

  std::move(callback).Run(HermesResponseStatus::kSuccess, profile_path,
                          service_path);
}

void CellularESimInstaller::OnPrepareCellularNetworkForConnectionFailure(
    const dbus::ObjectPath& profile_path,
    InstallProfileFromActivationCodeCallback callback,
    const std::string& service_path,
    const std::string& error_name) {
  HandleNewProfileEnableFailure(std::move(callback), profile_path, service_path,
                                error_name);
}

void CellularESimInstaller::HandleNewProfileEnableFailure(
    InstallProfileFromActivationCodeCallback callback,
    const dbus::ObjectPath& profile_path,
    const std::string& service_path,
    const std::string& error_name) {
  NET_LOG(ERROR) << "Error enabling newly created profile path="
                 << profile_path.value() << ", service path=" << service_path
                 << ", error_name=" << error_name;

  std::move(callback).Run(HermesResponseStatus::kErrorWrongState,
                          /*profile_path=*/absl::nullopt,
                          /*service_path=*/absl::nullopt);
}

}  // namespace chromeos