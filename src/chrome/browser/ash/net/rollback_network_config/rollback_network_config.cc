// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config.h"

#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_onc_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kDeviceUserHash[] = "";

bool IsOwnershipTaken() {
  return ash::DeviceSettingsService::Get()->GetOwnershipStatus() ==
         ash::DeviceSettingsService::OwnershipStatus::OWNERSHIP_TAKEN;
}

bool IsDeviceEnterpriseEnrolled() {
  return InstallAttributes::Get()->IsEnterpriseManaged();
}

bool OncIsPskWiFi(const base::Value& network) {
  return rollback_network_config::OncIsWiFi(network) &&
         rollback_network_config::OncWiFiIsPsk(network);
}

bool ShouldSaveNetwork(const base::Value& network) {
  return (rollback_network_config::OncIsSourceDevicePolicy(network) ||
          rollback_network_config::OncIsSourceDevice(network)) &&
         (rollback_network_config::OncHasNoSecurity(network) ||
          OncIsPskWiFi(network) ||
          rollback_network_config::OncIsEapWithoutClientCertificate(network));
}

void PrintError(const std::string& error_name,
                std::unique_ptr<base::DictionaryValue>) {
  LOG(ERROR) << error_name;
}

ManagedNetworkConfigurationHandler* managed_network_configuration_handler() {
  return NetworkHandler::Get()->managed_network_configuration_handler();
}

NetworkStateHandler* network_state_handler() {
  return NetworkHandler::Get()->network_state_handler();
}

// Configures the active part of a given managed onc network configuration as
// if it was a device wide user configured network. In particular this will
// configure policy-set values as user configured as well.
void ManagedOncConfigureActivePartAsDeviceWide(
    base::Value network,
    base::OnceCallback<void(bool)> callback) {
  rollback_network_config::ManagedOncCollapseToActive(&network);

  const std::string& guid = rollback_network_config::GetStringValue(
      network, onc::network_config::kGUID);
  const NetworkState* network_state =
      network_state_handler()->GetNetworkStateFromGuid(guid);

  auto callbacks = base::SplitOnceCallback(std::move(callback));
  auto success_callback = base::BindOnce(std::move(callbacks.first), true);
  auto failure_callback = base::BindOnce(std::move(callbacks.second), false);

  network.SetStringKey(onc::network_config::kSource,
                       onc::network_config::kSourceDevice);
  if (!network_state || !network_state->IsInProfile()) {
    managed_network_configuration_handler()->CreateConfiguration(
        kDeviceUserHash, base::Value::AsDictionaryValue(network),
        base::BindOnce([](const std::string&, const std::string&) {
        }).Then(std::move(success_callback)),
        base::BindOnce(&PrintError).Then(std::move(failure_callback)));
  } else if (network_state) {
    managed_network_configuration_handler()->SetProperties(
        network_state->path(), base::Value::AsDictionaryValue(network),
        std::move(success_callback),
        base::BindOnce(&PrintError).Then(std::move(failure_callback)));
  }
}

// Returns a list of all device wide wifi and ethernet networks, policy
// configured and user configured.
NetworkStateHandler::NetworkStateList GetDeviceWideWiFiAndEthernetNetworks() {
  NetworkStateHandler::NetworkStateList wifi;
  NetworkStateHandler::NetworkStateList ethernet;

  network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /*configured_only=*/true,
      /*visible_only=*/false, 0, &wifi);
  network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::EthernetOrEthernetEAP(),
      /*configured_only=*/true,
      /*visible_only=*/false, 0, &ethernet);

  NetworkStateHandler::NetworkStateList networks(wifi.begin(), wifi.end());
  networks.insert(networks.end(), ethernet.begin(), ethernet.end());

  return networks;
}

void ReconfigureUiData(const base::Value& network_config,
                       const std::string& guid) {
  const NetworkState* network_state =
      network_state_handler()->GetNetworkStateFromGuid(guid);

  base::Value ui_data = network_config.Clone();
  rollback_network_config::ManagedOncCollapseToUiData(&ui_data);

  managed_network_configuration_handler()->SetProperties(
      network_state->path(), base::Value::AsDictionaryValue(ui_data),
      base::DoNothing(), base::BindOnce(&PrintError));
}

using GetPasswordResult =
    base::OnceCallback<void(absl::optional<std::string> password,
                            const std::string& error_name,
                            const std::string& error_message)>;

void OnGetPassword(GetPasswordResult callback, const std::string& password) {
  std::move(callback).Run(password, /*error_name=*/std::string(),
                          /*error_message=*/std::string());
}

void OnGetPasswordError(GetPasswordResult callback,
                        const std::string& error_name,
                        const std::string& error_message) {
  std::move(callback).Run(absl::nullopt, error_name, error_message);
}

void GetPskPassword(GetPasswordResult callback,
                    const std::string& service_path) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  ShillServiceClient::Get()->GetWiFiPassphrase(
      dbus::ObjectPath(service_path),
      base::BindOnce(&OnGetPassword, std::move(split_callback.first)),
      base::BindOnce(&OnGetPasswordError, std::move(split_callback.second)));
}

void GetEapPassword(GetPasswordResult callback,
                    const std::string& service_path) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  ShillServiceClient::Get()->GetEapPassphrase(
      dbus::ObjectPath(service_path),
      base::BindOnce(&OnGetPassword, std::move(split_callback.first)),
      base::BindOnce(&OnGetPasswordError, std::move(split_callback.second)));
}

}  // namespace

class RollbackNetworkConfig::Exporter {
 public:
  Exporter();
  Exporter(const Exporter&) = delete;
  Exporter& operator=(const Exporter&) = delete;
  ~Exporter();

  void Export(ExportCallback callback);

 private:
  void OnGetManagedNetworkConfig(base::OnceClosure network_finished,
                                 const std::string& service_path,
                                 absl::optional<base::Value> properties,
                                 absl::optional<std::string> error);

  void AddPskPassword(base::ScopedClosureRunner exit_call,
                      int network_idx,
                      absl::optional<std::string> password,
                      const std::string& error_name,
                      const std::string& error_message);
  void AddEapPassword(base::ScopedClosureRunner exit_call,
                      int network_idx,
                      absl::optional<std::string> password,
                      const std::string& error_name,
                      const std::string& error_message);

  void SendNetworkConfigs(ExportCallback callback);
  std::string SerializeNetworkConfigs() const;

  std::vector<base::Value> network_configs_;
  ExportCallback callback_;

  base::WeakPtrFactory<Exporter> weak_factory_{this};
};

RollbackNetworkConfig::Exporter::Exporter() = default;
RollbackNetworkConfig::Exporter::~Exporter() = default;

void RollbackNetworkConfig::Exporter::Export(ExportCallback callback) {
  NetworkStateHandler::NetworkStateList networks =
      GetDeviceWideWiFiAndEthernetNetworks();

  auto finished_a_network = base::BarrierClosure(
      networks.size(),
      base::BindOnce(&Exporter::SendNetworkConfigs, weak_factory_.GetWeakPtr(),
                     std::move(callback)));

  for (auto* const network_state : networks) {
    managed_network_configuration_handler()->GetManagedProperties(
        kDeviceUserHash, network_state->path(),
        base::BindOnce(&Exporter::OnGetManagedNetworkConfig,
                       weak_factory_.GetWeakPtr(), finished_a_network));
  }
}

void RollbackNetworkConfig::Exporter::OnGetManagedNetworkConfig(
    base::OnceClosure network_finished,
    const std::string& service_path,
    absl::optional<base::Value> managed_network,
    absl::optional<std::string> error) {
  base::ScopedClosureRunner exit_call(std::move(network_finished));

  if (!managed_network.has_value()) {
    LOG(ERROR) << "Failed to fetch properties for " << service_path
               << ", error=" << error.value_or("(no error)");
    return;
  }

  base::Value active_network = managed_network->Clone();
  rollback_network_config::ManagedOncCollapseToActive(&active_network);

  if (!ShouldSaveNetwork(active_network)) {
    return;
  }

  network_configs_.push_back(std::move(*managed_network));
  int network_idx = network_configs_.size() - 1;

  if (OncIsPskWiFi(active_network)) {
    GetPskPassword(
        base::BindOnce(&RollbackNetworkConfig::Exporter::AddPskPassword,
                       weak_factory_.GetWeakPtr(), std::move(exit_call),
                       network_idx),
        service_path);
  } else if (rollback_network_config::OncIsEapWithoutClientCertificate(
                 active_network)) {
    GetEapPassword(
        base::BindOnce(&RollbackNetworkConfig::Exporter::AddEapPassword,
                       weak_factory_.GetWeakPtr(), std::move(exit_call),
                       network_idx),
        service_path);
  }
}

void RollbackNetworkConfig::Exporter::AddPskPassword(
    base::ScopedClosureRunner exit_call,
    int network_idx,
    absl::optional<std::string> password,
    const std::string& error_name,
    const std::string& error_message) {
  if (!password) {
    LOG(ERROR) << error_name << " " << error_message;
    return;
  }
  base::Value* network = &network_configs_[network_idx];
  rollback_network_config::ManagedOncWiFiSetPskPassword(network, *password);
}

void RollbackNetworkConfig::Exporter::AddEapPassword(
    base::ScopedClosureRunner exit_call,
    int network_idx,
    absl::optional<std::string> password,
    const std::string& error_name,
    const std::string& error_message) {
  if (!password) {
    LOG(ERROR) << error_name << " " << error_message;
    return;
  }
  base::Value* network = &network_configs_[network_idx];
  rollback_network_config::ManagedOncSetEapPassword(network, *password);
}

void RollbackNetworkConfig::Exporter::SendNetworkConfigs(
    ExportCallback callback) {
  std::move(callback).Run(SerializeNetworkConfigs());
}

std::string RollbackNetworkConfig::Exporter::SerializeNetworkConfigs() const {
  base::Value network_config_list(base::Value::Type::LIST);
  for (const auto& network_config : network_configs_) {
    network_config_list.Append(network_config.Clone());
  }

  base::Value complete_network_configuration(base::Value::Type::DICTIONARY);
  complete_network_configuration.SetKey(
      onc::toplevel_config::kNetworkConfigurations,
      std::move(network_config_list));
  std::string serialized_config;
  base::JSONWriter::Write(complete_network_configuration, &serialized_config);
  return serialized_config;
}

class RollbackNetworkConfig::Importer
    : public ash::DeviceSettingsService::Observer,
      public NetworkPolicyObserver {
 public:
  Importer();
  Importer(const Importer&) = delete;
  Importer& operator=(const Importer&) = delete;
  ~Importer() override;

  void Import(const std::string& config, ImportCallback callback);

  // DeviceSettingsService::Observer
  void OwnershipStatusChanged() override;

  // NetworkPolicyObserver
  void PoliciesApplied(const std::string& userhash) override;

  void set_skip_ownership_check_for_testing() {
    skip_ownership_check_for_testing_ = true;
  }

 private:
  void DeleteImportedPolicyNetworks();

  void NetworkConfigured(base::RepeatingClosure closure, bool success);
  void AllNetworksConfigured(ImportCallback callback);

  std::vector<base::Value> imported_networks_;

  bool all_networks_successfully_configured = true;

  bool skip_ownership_check_for_testing_ = false;

  base::WeakPtrFactory<Importer> weak_factory_{this};
};

RollbackNetworkConfig::Importer::Importer() {
  ash::DeviceSettingsService::Get()->AddObserver(this);
  chromeos::NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->AddObserver(this);
}

RollbackNetworkConfig::Importer::~Importer() {
  if (ash::DeviceSettingsService::Get()) {
    ash::DeviceSettingsService::Get()->RemoveObserver(this);
  }
  if (chromeos::NetworkHandler::Get()) {
    chromeos::NetworkHandler::Get()
        ->managed_network_configuration_handler()
        ->RemoveObserver(this);
  }
}

void RollbackNetworkConfig::Importer::Import(const std::string& network_config,
                                             ImportCallback callback) {
  absl::optional<base::Value> managed_onc_network_config =
      base::JSONReader::Read(network_config);

  if (!managed_onc_network_config.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  base::Value* network_list = managed_onc_network_config->FindListKey(
      onc::toplevel_config::kNetworkConfigurations);
  if (!network_list || !network_list->is_list()) {
    std::move(callback).Run(false);
    return;
  }

  auto barrier_closure = base::BarrierClosure(
      network_list->GetList().size(),
      base::BindOnce(&RollbackNetworkConfig::Importer::AllNetworksConfigured,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  auto finished_a_network =
      base::BindRepeating(&RollbackNetworkConfig::Importer::NetworkConfigured,
                          weak_factory_.GetWeakPtr(), barrier_closure);

  bool ownership_taken = IsOwnershipTaken();

  for (base::Value& network : network_list->GetList()) {
    if (!ownership_taken) {
      ManagedOncConfigureActivePartAsDeviceWide(network.Clone(),
                                                finished_a_network);
    }
    // If ownership is taken already we may still be waiting for policy
    // application to finish. Once it's finished, `PoliciesApplied`
    // reconfigures the networks.
    imported_networks_.push_back(std::move(network));
  }
}

void RollbackNetworkConfig::Importer::OwnershipStatusChanged() {
  if ((skip_ownership_check_for_testing_ || IsOwnershipTaken()) &&
      !IsDeviceEnterpriseEnrolled()) {
    LOG(WARNING)
        << "Device was not enrolled, deleting all policy imported networks.";
    DeleteImportedPolicyNetworks();
  }
}

void RollbackNetworkConfig::Importer::PoliciesApplied(
    const std::string& userhash) {
  for (const base::Value& network_config : imported_networks_) {
    const std::string& guid = rollback_network_config::GetStringValue(
        network_config, onc::network_config::kGUID);
    const NetworkState* network_state =
        network_state_handler()->GetNetworkStateFromGuid(guid);

    if (network_state &&
        rollback_network_config::OncIsSourceDevicePolicy(network_config)) {
      if (network_state->IsManagedByPolicy()) {
        ReconfigureUiData(network_config, guid);
      } else {  // Policy did not reconfigure the network, delete it.
        managed_network_configuration_handler()->RemoveConfiguration(
            network_state->path(), base::DoNothing(),
            base::BindOnce(&PrintError));
      }
    }
  }
}

void RollbackNetworkConfig::Importer::DeleteImportedPolicyNetworks() {
  for (const base::Value& network_config : imported_networks_) {
    const std::string& guid = rollback_network_config::GetStringValue(
        network_config, onc::network_config::kGUID);

    if (rollback_network_config::OncIsSourceDevicePolicy(network_config)) {
      const NetworkState* network_state =
          network_state_handler()->GetNetworkStateFromGuid(guid);

      if (network_state && !network_state->profile_path().empty()) {
        managed_network_configuration_handler()->RemoveConfiguration(
            network_state->path(), base::DoNothing(),
            base::BindOnce(&PrintError));
      }
    }
  }
}

void RollbackNetworkConfig::Importer::NetworkConfigured(
    base::RepeatingClosure closure,
    bool success) {
  all_networks_successfully_configured &= success;
  closure.Run();
}

void RollbackNetworkConfig::Importer::AllNetworksConfigured(
    ImportCallback callback) {
  std::move(callback).Run(all_networks_successfully_configured);
}

RollbackNetworkConfig::RollbackNetworkConfig() = default;
RollbackNetworkConfig::~RollbackNetworkConfig() = default;

void RollbackNetworkConfig::BindReceiver(
    mojo::PendingReceiver<
        chromeos::rollback_network_config::mojom::RollbackNetworkConfig>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void RollbackNetworkConfig::RollbackConfigImport(
    const std::string& network_config,
    ImportCallback callback) {
  // There should only be one import request.
  if (importer_) {
    LOG(WARNING) << "There was another import request before.";
  }
  importer_ = std::make_unique<Importer>();
  importer_->Import(network_config, std::move(callback));
}

void RollbackNetworkConfig::RollbackConfigExport(ExportCallback callback) {
  // There should only be one export request.
  if (exporter_) {
    LOG(WARNING) << "There was another export request before.";
  }
  exporter_ = std::make_unique<Exporter>();
  exporter_->Export(std::move(callback));
}

void RollbackNetworkConfig::fake_ownership_taken_for_testing() {
  importer_->set_skip_ownership_check_for_testing();  // IN-TEST
  importer_->OwnershipStatusChanged();
}

}  // namespace ash
