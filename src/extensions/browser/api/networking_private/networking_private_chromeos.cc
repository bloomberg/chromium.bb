// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/onc/onc_constants.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkCertificateHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;
using extensions::NetworkingPrivateDelegate;

namespace private_api = extensions::api::networking_private;

namespace {

chromeos::NetworkStateHandler* GetStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

chromeos::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return NetworkHandler::Get()->managed_network_configuration_handler();
}

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path,
                            std::string* error) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    *error = extensions::networking_private::kErrorInvalidNetworkGuid;
    return false;
  }
  *service_path = network->path();
  return true;
}

bool IsSharedNetwork(const std::string& service_path) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, true /* configured only */);
  if (!network)
    return false;

  return !network->IsPrivate();
}

bool GetPrimaryUserIdHash(content::BrowserContext* browser_context,
                          std::string* user_hash,
                          std::string* error) {
  std::string context_user_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context);

  // Currently Chrome OS only configures networks for the primary user.
  // Configuration attempts from other browser contexts should fail.
  if (context_user_hash != chromeos::LoginState::Get()->primary_user_hash()) {
    // Disallow class requiring a user id hash from a non-primary user context
    // to avoid complexities with the policy code.
    LOG(ERROR) << "networkingPrivate API call from non primary user: "
               << context_user_hash;
    if (error)
      *error = "Error.NonPrimaryUser";
    return false;
  }
  if (user_hash)
    *user_hash = context_user_hash;
  return true;
}

void AppendDeviceState(
    const std::string& type,
    const chromeos::DeviceState* device,
    NetworkingPrivateDelegate::DeviceStateList* device_state_list) {
  DCHECK(!type.empty());
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);
  NetworkStateHandler::TechnologyState technology_state =
      GetStateHandler()->GetTechnologyState(pattern);
  private_api::DeviceStateType state = private_api::DEVICE_STATE_TYPE_NONE;
  switch (technology_state) {
    case NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      if (!device)
        return;
      // If we have a DeviceState entry but the technology is not available,
      // assume the technology is not initialized.
      state = private_api::DEVICE_STATE_TYPE_UNINITIALIZED;
      break;
    case NetworkStateHandler::TECHNOLOGY_AVAILABLE:
      state = private_api::DEVICE_STATE_TYPE_DISABLED;
      break;
    case NetworkStateHandler::TECHNOLOGY_DISABLING:
      state = private_api::DEVICE_STATE_TYPE_DISABLED;
      break;
    case NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
      state = private_api::DEVICE_STATE_TYPE_UNINITIALIZED;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLING:
      state = private_api::DEVICE_STATE_TYPE_ENABLING;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLED:
      state = private_api::DEVICE_STATE_TYPE_ENABLED;
      break;
    case NetworkStateHandler::TECHNOLOGY_PROHIBITED:
      state = private_api::DEVICE_STATE_TYPE_PROHIBITED;
      break;
  }
  DCHECK_NE(private_api::DEVICE_STATE_TYPE_NONE, state);
  std::unique_ptr<private_api::DeviceStateProperties> properties(
      new private_api::DeviceStateProperties);
  properties->type = private_api::ParseNetworkType(type);
  properties->state = state;
  if (device && state == private_api::DEVICE_STATE_TYPE_ENABLED)
    properties->scanning.reset(new bool(device->scanning()));
  if (device && type == ::onc::network_config::kCellular) {
    bool sim_present = !device->IsSimAbsent();
    properties->sim_present = std::make_unique<bool>(sim_present);
    if (sim_present) {
      auto sim_lock_status = std::make_unique<private_api::SIMLockStatus>();
      sim_lock_status->lock_enabled = device->sim_lock_enabled();
      sim_lock_status->lock_type = device->sim_lock_type();
      sim_lock_status->retries_left.reset(new int(device->sim_retries_left()));
      properties->sim_lock_status = std::move(sim_lock_status);
    }
  }
  if (device && type == ::onc::network_config::kWiFi) {
    properties->managed_network_available = std::make_unique<bool>(
        GetStateHandler()->GetAvailableManagedWifiNetwork());
  }
  device_state_list->push_back(std::move(properties));
}

void NetworkHandlerFailureCallback(
    const NetworkingPrivateDelegate::FailureCallback& callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  callback.Run(error_name);
}

// Returns the string corresponding to |key|. If the property is a managed
// dictionary, returns the active value. If the property does not exist or
// has no active value, returns an empty string.
std::string GetStringFromDictionary(const base::DictionaryValue& dictionary,
                                    const std::string& key) {
  std::string result;
  if (!dictionary.GetStringWithoutPathExpansion(key, &result)) {
    const base::DictionaryValue* managed = nullptr;
    if (dictionary.GetDictionaryWithoutPathExpansion(key, &managed)) {
      managed->GetStringWithoutPathExpansion(::onc::kAugmentationActiveSetting,
                                             &result);
    }
  }
  return result;
}

base::DictionaryValue* GetThirdPartyVPNDictionary(
    base::DictionaryValue* dictionary) {
  const std::string type =
      GetStringFromDictionary(*dictionary, ::onc::network_config::kType);
  if (type != ::onc::network_config::kVPN)
    return nullptr;
  base::DictionaryValue* vpn_dict = nullptr;
  if (!dictionary->GetDictionary(::onc::network_config::kVPN, &vpn_dict))
    return nullptr;
  if (GetStringFromDictionary(*vpn_dict, ::onc::vpn::kType) !=
      ::onc::vpn::kThirdPartyVpn) {
    return nullptr;
  }
  base::DictionaryValue* third_party_vpn = nullptr;
  vpn_dict->GetDictionary(::onc::vpn::kThirdPartyVpn, &third_party_vpn);
  return third_party_vpn;
}

const chromeos::DeviceState* GetCellularDeviceState(const std::string& guid) {
  const chromeos::NetworkState* network_state = nullptr;
  if (!guid.empty())
    network_state = GetStateHandler()->GetNetworkStateFromGuid(guid);
  const chromeos::DeviceState* device_state = nullptr;
  if (network_state) {
    device_state =
        GetStateHandler()->GetDeviceState(network_state->device_path());
  }
  if (!device_state) {
    device_state =
        GetStateHandler()->GetDeviceStateByType(NetworkTypePattern::Cellular());
  }
  return device_state;
}

private_api::Certificate GetCertDictionary(
    const NetworkCertificateHandler::Certificate& cert) {
  private_api::Certificate api_cert;
  api_cert.hash = cert.hash;
  api_cert.issued_by = cert.issued_by;
  api_cert.issued_to = cert.issued_to;
  api_cert.hardware_backed = cert.hardware_backed;
  api_cert.device_wide = cert.device_wide;
  if (!cert.pem.empty())
    api_cert.pem = std::make_unique<std::string>(cert.pem);
  if (!cert.pkcs11_id.empty())
    api_cert.pkcs11_id = std::make_unique<std::string>(cert.pkcs11_id);
  return api_cert;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

namespace extensions {

NetworkingPrivateChromeOS::NetworkingPrivateChromeOS(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

NetworkingPrivateChromeOS::~NetworkingPrivateChromeOS() {}

void NetworkingPrivateChromeOS::GetProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  std::string user_id_hash;
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, service_path,
      base::BindOnce(&NetworkingPrivateChromeOS::GetPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(), guid, false /* managed */,
                     success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetManagedProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  std::string user_id_hash;
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->GetManagedProperties(
      user_id_hash, service_path,
      base::BindOnce(&NetworkingPrivateChromeOS::GetPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(), guid, true /* managed */,
                     success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetState(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, false /* configured_only */);
  if (!network_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }

  std::unique_ptr<base::DictionaryValue> network_properties =
      chromeos::network_util::TranslateNetworkStateToONC(network_state);
  AppendThirdPartyProviderName(network_properties.get());

  success_callback.Run(std::move(network_properties));
}

void NetworkingPrivateChromeOS::SetProperties(
    const std::string& guid,
    std::unique_ptr<base::DictionaryValue> properties,
    bool allow_set_shared_config,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    failure_callback.Run(
        extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }
  if (network->profile_path().empty()) {
    failure_callback.Run(
        extensions::networking_private::kErrorUnconfiguredNetwork);
    return;
  }
  if (IsSharedNetwork(network->path())) {
    if (!allow_set_shared_config) {
      failure_callback.Run(networking_private::kErrorAccessToSharedConfig);
      return;
    }
  } else {
    std::string user_id_hash;
    std::string error;
    // Do not allow changing a non-shared network from a secondary users.
    if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
      failure_callback.Run(error);
      return;
    }
  }

  NET_LOG(USER) << "networkingPrivate.setProperties for: "
                << NetworkId(network);
  GetManagedConfigurationHandler()->SetProperties(
      network->path(), *properties, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkHandlerCreateCallback(
    const NetworkingPrivateDelegate::StringCallback& callback,
    const std::string& service_path,
    const std::string& guid) {
  callback.Run(guid);
}

void NetworkingPrivateChromeOS::CreateNetwork(
    bool shared,
    std::unique_ptr<base::DictionaryValue> properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string user_id_hash, error;
  // Do not allow configuring a non-shared network from a non-primary user.
  if (!shared &&
      !GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  const std::string guid =
      GetStringFromDictionary(*properties, ::onc::network_config::kGUID);
  NET_LOG(USER) << "networkingPrivate.CreateNetwork. GUID=" << guid;
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::Bind(&NetworkHandlerCreateCallback, success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::ForgetNetwork(
    const std::string& guid,
    bool allow_forget_shared_config,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, true /* configured only */);
  if (!network) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }

  std::string user_id_hash;
  // Don't allow non-primary user to remove private configs - the private
  // configs belong to the primary user (non-primary users' network configs
  // never get loaded by shill).
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error) &&
      network->IsPrivate()) {
    failure_callback.Run(error);
    return;
  }

  if (!allow_forget_shared_config && !network->IsPrivate()) {
    failure_callback.Run(networking_private::kErrorAccessToSharedConfig);
    return;
  }

  onc::ONCSource onc_source = onc::ONC_SOURCE_UNKNOWN;
  if (GetManagedConfigurationHandler()->FindPolicyByGUID(user_id_hash, guid,
                                                         &onc_source)) {
    // Prevent a policy controlled configuration removal.
    if (onc_source == onc::ONC_SOURCE_DEVICE_POLICY) {
      allow_forget_shared_config = false;
    } else {
      failure_callback.Run(networking_private::kErrorPolicyControlled);
      return;
    }
  }

  if (allow_forget_shared_config) {
    GetManagedConfigurationHandler()->RemoveConfiguration(
        service_path, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  } else {
    GetManagedConfigurationHandler()->RemoveConfigurationFromCurrentProfile(
        service_path, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  }
}

void NetworkingPrivateChromeOS::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    const NetworkListCallback& success_callback,
    const FailureCallback& failure_callback) {
  // When requesting configured Ethernet networks, include EthernetEAP.
  NetworkTypePattern pattern =
      (!visible_only && network_type == ::onc::network_type::kEthernet)
          ? NetworkTypePattern::EthernetOrEthernetEAP()
          : chromeos::onc::NetworkTypePatternFromOncType(network_type);
  std::unique_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          pattern, configured_only, visible_only, limit);

  for (auto& value : *network_properties_list) {
    base::DictionaryValue* network_dict = nullptr;
    value.GetAsDictionary(&network_dict);
    DCHECK(network_dict);
    if (GetThirdPartyVPNDictionary(network_dict))
      AppendThirdPartyProviderName(network_dict);
  }

  success_callback.Run(std::move(network_properties_list));
}

void NetworkingPrivateChromeOS::StartConnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback),
      true /* check_error_state */, chromeos::ConnectCallbackMode::ON_STARTED);
}

void NetworkingPrivateChromeOS::StartDisconnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::StartActivate(
    const std::string& guid,
    const std::string& specified_carrier,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    failure_callback.Run(
        extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }

  if (ui_delegate())
    ui_delegate()->ShowAccountDetails(guid);
  success_callback.Run();
}

void NetworkingPrivateChromeOS::SetWifiTDLSEnabledState(
    const std::string& ip_or_mac_address,
    bool enabled,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkHandler::Get()->network_device_handler()->SetWifiTDLSEnabled(
      ip_or_mac_address, enabled, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkHandler::Get()->network_device_handler()->GetWifiTDLSStatus(
      ip_or_mac_address, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetCaptivePortalStatus(
    const std::string& guid,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!chromeos::network_portal_detector::IsInitialized()) {
    failure_callback.Run(networking_private::kErrorNotReady);
    return;
  }

  success_callback.Run(
      chromeos::NetworkPortalDetector::CaptivePortalStatusString(
          chromeos::network_portal_detector::GetInstance()
              ->GetCaptivePortalState(guid)
              .status));
}

void NetworkingPrivateChromeOS::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  std::string lock_type = device_state->sim_lock_type();
  if (lock_type.empty()) {
    // Sim is already unlocked.
    failure_callback.Run(networking_private::kErrorInvalidNetworkOperation);
    return;
  }

  // Unblock or unlock the SIM.
  if (lock_type == shill::kSIMLockPuk) {
    NetworkHandler::Get()->network_device_handler()->UnblockPin(
        device_state->path(), puk, pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  } else {
    NetworkHandler::Get()->network_device_handler()->EnterPin(
        device_state->path(), pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  }
}

void NetworkingPrivateChromeOS::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  if (!device_state->sim_lock_type().empty()) {
    // The SIM needs to be unlocked before the state can be changed.
    failure_callback.Run(networking_private::kErrorSimLocked);
    return;
  }

  // TODO(benchan): Add more checks to validate the parameters of this method
  // and the state of the SIM lock on the cellular device. Consider refactoring
  // some of the code by moving the logic into shill instead.

  // If |new_pin| is empty, we're trying to enable (require_pin == true) or
  // disable (require_pin == false) SIM locking.
  if (new_pin.empty()) {
    NetworkHandler::Get()->network_device_handler()->RequirePin(
        device_state->path(), require_pin, current_pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
    return;
  }

  // Otherwise, we're trying to change the PIN from |current_pin| to
  // |new_pin|, which also requires SIM locking to be enabled, i.e.
  // require_pin == true.
  if (!require_pin) {
    failure_callback.Run(networking_private::kErrorInvalidArguments);
    return;
  }

  NetworkHandler::Get()->network_device_handler()->ChangePin(
      device_state->path(), current_pin, new_pin, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  NetworkHandler::Get()->network_device_handler()->RegisterCellularNetwork(
      device_state->path(), network_id, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

std::unique_ptr<base::ListValue>
NetworkingPrivateChromeOS::GetEnabledNetworkTypes() {
  chromeos::NetworkStateHandler* state_handler = GetStateHandler();

  std::unique_ptr<base::ListValue> network_list(new base::ListValue);

  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Ethernet()))
    network_list->AppendString(::onc::network_type::kEthernet);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()))
    network_list->AppendString(::onc::network_type::kWiFi);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()))
    network_list->AppendString(::onc::network_type::kCellular);

  return network_list;
}

std::unique_ptr<NetworkingPrivateDelegate::DeviceStateList>
NetworkingPrivateChromeOS::GetDeviceStateList() {
  std::set<std::string> technologies_found;
  NetworkStateHandler::DeviceStateList devices;
  NetworkHandler::Get()->network_state_handler()->GetDeviceList(&devices);

  std::unique_ptr<DeviceStateList> device_state_list(new DeviceStateList);
  for (const chromeos::DeviceState* device : devices) {
    std::string onc_type =
        chromeos::network_util::TranslateShillTypeToONC(device->type());
    AppendDeviceState(onc_type, device, device_state_list.get());
    technologies_found.insert(onc_type);
  }

  // For any technologies that we do not have a DeviceState entry for, append
  // an entry if the technology is available.
  const char* technology_types[] = {::onc::network_type::kEthernet,
                                    ::onc::network_type::kWiFi,
                                    ::onc::network_type::kCellular};
  for (const char* technology : technology_types) {
    if (base::Contains(technologies_found, technology))
      continue;
    AppendDeviceState(technology, nullptr /* device */,
                      device_state_list.get());
  }
  return device_state_list;
}

std::unique_ptr<base::DictionaryValue>
NetworkingPrivateChromeOS::GetGlobalPolicy() {
  auto result = std::make_unique<base::DictionaryValue>();
  const base::DictionaryValue* global_network_config =
      GetManagedConfigurationHandler()->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (global_network_config)
    result->MergeDictionary(global_network_config);
  return result;
}

std::unique_ptr<base::DictionaryValue>
NetworkingPrivateChromeOS::GetCertificateLists() {
  private_api::CertificateLists result;
  const std::vector<NetworkCertificateHandler::Certificate>& server_cas =
      NetworkHandler::Get()
          ->network_certificate_handler()
          ->server_ca_certificates();
  for (const auto& cert : server_cas)
    result.server_ca_certificates.push_back(GetCertDictionary(cert));

  std::vector<private_api::Certificate> user_cert_list;
  const std::vector<NetworkCertificateHandler::Certificate>& user_certs =
      NetworkHandler::Get()
          ->network_certificate_handler()
          ->client_certificates();
  for (const auto& cert : user_certs)
    result.user_certificates.push_back(GetCertDictionary(cert));

  return result.ToValue();
}

bool NetworkingPrivateChromeOS::EnableNetworkType(const std::string& type) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);

  GetStateHandler()->SetTechnologyEnabled(
      pattern, true, chromeos::network_handler::ErrorCallback());

  return true;
}

bool NetworkingPrivateChromeOS::DisableNetworkType(const std::string& type) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);

  GetStateHandler()->SetTechnologyEnabled(
      pattern, false, chromeos::network_handler::ErrorCallback());

  return true;
}

bool NetworkingPrivateChromeOS::RequestScan(const std::string& type) {
  NetworkTypePattern pattern = chromeos::onc::NetworkTypePatternFromOncType(
      type.empty() ? ::onc::network_type::kAllTypes : type);
  GetStateHandler()->RequestScan(pattern);
  return true;
}

// Private methods

void NetworkingPrivateChromeOS::GetPropertiesCallback(
    const std::string& guid,
    bool managed,
    const DictionaryCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  std::unique_ptr<base::DictionaryValue> dictionary_copy =
      dictionary.CreateDeepCopy();
  AppendThirdPartyProviderName(dictionary_copy.get());
  callback.Run(std::move(dictionary_copy));
}

void NetworkingPrivateChromeOS::AppendThirdPartyProviderName(
    base::DictionaryValue* dictionary) {
  base::DictionaryValue* third_party_vpn =
      GetThirdPartyVPNDictionary(dictionary);
  if (!third_party_vpn)
    return;

  const std::string extension_id = GetStringFromDictionary(
      *third_party_vpn, ::onc::third_party_vpn::kExtensionID);
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  for (const auto& extension : extensions) {
    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kVpnProvider) &&
        extension->id() == extension_id) {
      third_party_vpn->SetKey(::onc::third_party_vpn::kProviderName,
                              base::Value(extension->name()));
      break;
    }
  }
}

}  // namespace extensions
