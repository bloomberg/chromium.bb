// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "chromeos/network/tether_constants.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kErrorUnknown[] = "Unknown";

const char kDefaultCellularNetworkPath[] = "/cellular";

std::string GetStringFromDictionary(const base::Value* dict, const char* key) {
  const base::Value* v = dict ? dict->FindKey(key) : nullptr;
  return v ? v->GetString() : std::string();
}

bool IsCaptivePortalState(const base::Value& properties, bool log) {
  std::string state =
      GetStringFromDictionary(&properties, shill::kStateProperty);
  if (state != shill::kStatePortal)
    return false;
  if (!properties.FindKey(shill::kPortalDetectionFailedPhaseProperty) ||
      !properties.FindKey(shill::kPortalDetectionFailedStatusProperty)) {
    // If Shill (or a stub) has not set PortalDetectionFailedStatus
    // or PortalDetectionFailedPhase, assume we are in captive portal state.
    return true;
  }

  std::string portal_detection_phase = GetStringFromDictionary(
      &properties, shill::kPortalDetectionFailedPhaseProperty);
  std::string portal_detection_status = GetStringFromDictionary(
      &properties, shill::kPortalDetectionFailedStatusProperty);

  // Shill reports the phase in which it determined that the device is behind a
  // captive portal. We only want to rely only on incorrect content being
  // returned and ignore other reasons.
  bool is_captive_portal =
      portal_detection_phase == shill::kPortalDetectionPhaseContent &&
      portal_detection_status == shill::kPortalDetectionStatusFailure;

  if (log) {
    std::string name =
        GetStringFromDictionary(&properties, shill::kNameProperty);
    if (name.empty())
      name = GetStringFromDictionary(&properties, shill::kSSIDProperty);
    if (!is_captive_portal) {
      NET_LOG(EVENT) << "State is 'portal' but not in captive portal state:"
                     << " name=" << name << " phase=" << portal_detection_phase
                     << " status=" << portal_detection_status;
    } else {
      NET_LOG(EVENT) << "Network is in captive portal state: " << name;
    }
  }

  return is_captive_portal;
}

}  // namespace

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path) {}

NetworkState::~NetworkState() = default;

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  // Keep care that these properties are the same as in |GetProperties|.
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == shill::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == shill::kStateProperty) {
    std::string saved_state = connection_state_;
    if (GetStringValue(key, value, &connection_state_)) {
      if (connection_state_ != saved_state)
        last_connection_state_ = saved_state;
      return true;
    } else {
      return false;
    }
  } else if (key == shill::kVisibleProperty) {
    return GetBooleanValue(key, value, &visible_);
  } else if (key == shill::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == shill::kErrorProperty) {
    if (!GetStringValue(key, value, &error_))
      return false;
    if (ErrorIsValid(error_))
      last_error_ = error_;
    else
      error_.clear();
    return true;
  } else if (key == shill::kWifiFrequency) {
    return GetIntegerValue(key, value, &frequency_);
  } else if (key == shill::kActivationTypeProperty) {
    return GetStringValue(key, value, &activation_type_);
  } else if (key == shill::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == shill::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == shill::kPaymentPortalProperty) {
    if (!value.is_dict())
      return false;
    const base::Value* portal_url_value = value.FindKeyOfType(
        shill::kPaymentPortalURL, base::Value::Type::STRING);
    if (!portal_url_value)
      return false;
    payment_url_ = portal_url_value->GetString();
  } else if (key == shill::kSecurityClassProperty) {
    return GetStringValue(key, value, &security_class_);
  } else if (key == shill::kEapMethodProperty) {
    return GetStringValue(key, value, &eap_method_);
  } else if (key == shill::kEapKeyMgmtProperty) {
    return GetStringValue(key, value, &eap_key_mgmt_);
  } else if (key == shill::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &network_technology_);
  } else if (key == shill::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == shill::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == shill::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kWifiHexSsid) {
    std::string ssid_hex;
    if (!GetStringValue(key, value, &ssid_hex))
      return false;
    raw_ssid_.clear();
    return base::HexStringToBytes(ssid_hex, &raw_ssid_);
  } else if (key == shill::kWifiBSsid) {
    return GetStringValue(key, value, &bssid_);
  } else if (key == shill::kPriorityProperty) {
    return GetIntegerValue(key, value, &priority_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  } else if (key == shill::kProxyConfigProperty) {
    std::string proxy_config_str;
    if (!value.GetAsString(&proxy_config_str)) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }

    proxy_config_.reset();
    if (proxy_config_str.empty())
      return true;

    std::unique_ptr<base::Value> proxy_config_dict(
        onc::ReadDictionaryFromJson(proxy_config_str));
    if (proxy_config_dict) {
      proxy_config_ = std::move(proxy_config_dict);
    } else {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
    }
    return true;
  } else if (key == shill::kProviderProperty) {
    const base::Value* type_value =
        value.is_dict() ? value.FindKeyOfType(shill::kTypeProperty,
                                              base::Value::Type::STRING)
                        : nullptr;
    if (!type_value) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }
    std::string vpn_provider_type = type_value->GetString();
    std::string vpn_provider_id;
    if (vpn_provider_type == shill::kProviderThirdPartyVpn ||
        vpn_provider_type == shill::kProviderArcVpn) {
      // If the network uses a third-party or Arc VPN provider,
      // |shill::kHostProperty| contains the extension ID or Arc package name.
      const base::Value* host_value =
          value.FindKeyOfType(shill::kHostProperty, base::Value::Type::STRING);
      if (!host_value) {
        NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
        return false;
      }
      vpn_provider_id = host_value->GetString();
    }
    SetVpnProvider(vpn_provider_id, vpn_provider_type);
    return true;
  } else if (key == shill::kTetheringProperty) {
    return GetStringValue(key, value, &tethering_state_);
  } else if (key == shill::kUIDataProperty) {
    std::unique_ptr<NetworkUIData> ui_data =
        chromeos::shill_property_util::GetUIDataFromValue(value);
    if (!ui_data)
      return false;
    onc_source_ = ui_data->onc_source();
    return true;
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(const base::Value& properties) {
  NET_LOG(EVENT) << "InitialPropertiesReceived: " << name() << " (" << path()
                 << ") State: " << connection_state_
                 << " Visible: " << visible_;
  if (!properties.FindKey(shill::kTypeProperty)) {
    NET_LOG(ERROR) << "NetworkState has no type: "
                   << shill_property_util::GetNetworkIdFromProperties(
                          properties);
    return false;
  }

  // By convention, all visible WiFi and WiMAX networks have a
  // SignalStrength > 0.
  if ((type() == shill::kTypeWifi || type() == shill::kTypeWimax) &&
      visible() && signal_strength_ <= 0) {
    signal_strength_ = 1;
  }

  // Any change to connection state will trigger a complete property update,
  // so we update is_captive_portal_ here.
  is_captive_portal_ = IsCaptivePortalState(properties, true /* log */);

  // Ensure that the network has a valid name.
  return UpdateName(properties);
}

void NetworkState::GetStateProperties(base::Value* dictionary) const {
  ManagedState::GetStateProperties(dictionary);

  // Properties shared by all types.
  dictionary->SetKey(shill::kGuidProperty, base::Value(guid()));
  dictionary->SetKey(shill::kSecurityClassProperty,
                     base::Value(security_class()));
  dictionary->SetKey(shill::kProfileProperty, base::Value(profile_path()));
  dictionary->SetKey(shill::kPriorityProperty, base::Value(priority_));

  if (visible())
    dictionary->SetKey(shill::kStateProperty, base::Value(connection_state()));
  if (!device_path().empty())
    dictionary->SetKey(shill::kDeviceProperty, base::Value(device_path()));

  // VPN properties.
  if (NetworkTypePattern::VPN().MatchesType(type()) && vpn_provider()) {
    // Shill sends VPN provider properties in a nested dictionary. |dictionary|
    // must replicate that nested structure.
    std::string provider_type = vpn_provider()->type;
    base::Value provider_property(base::Value::Type::DICTIONARY);
    provider_property.SetKey(shill::kTypeProperty, base::Value(provider_type));
    if (provider_type == shill::kProviderThirdPartyVpn ||
        provider_type == shill::kProviderArcVpn) {
      provider_property.SetKey(shill::kHostProperty,
                               base::Value(vpn_provider()->id));
    }
    dictionary->SetKey(shill::kProviderProperty, std::move(provider_property));
  }

  // Tether properties
  if (NetworkTypePattern::Tether().MatchesType(type())) {
    dictionary->SetKey(kTetherBatteryPercentage,
                       base::Value(battery_percentage()));
    dictionary->SetKey(kTetherCarrier, base::Value(carrier()));
    dictionary->SetKey(kTetherHasConnectedToHost,
                       base::Value(tether_has_connected_to_host()));
    dictionary->SetKey(kTetherSignalStrength, base::Value(signal_strength()));

    // Tether networks do not share some of the wireless/mobile properties added
    // below; exit early to avoid having these properties applied.
    return;
  }

  // Wireless properties
  if (!NetworkTypePattern::Wireless().MatchesType(type()))
    return;

  if (visible()) {
    dictionary->SetKey(shill::kConnectableProperty, base::Value(connectable()));
    dictionary->SetKey(shill::kSignalStrengthProperty,
                       base::Value(signal_strength()));
  }

  // Wifi properties
  if (NetworkTypePattern::WiFi().MatchesType(type())) {
    dictionary->SetKey(shill::kWifiBSsid, base::Value(bssid_));
    dictionary->SetKey(shill::kEapMethodProperty, base::Value(eap_method()));
    dictionary->SetKey(shill::kWifiFrequency, base::Value(frequency_));
    dictionary->SetKey(shill::kWifiHexSsid, base::Value(GetHexSsid()));
    dictionary->SetKey(shill::kTetheringProperty,
                       base::Value(tethering_state_));
  }

  // Mobile properties
  if (NetworkTypePattern::Mobile().MatchesType(type())) {
    dictionary->SetKey(shill::kNetworkTechnologyProperty,
                       base::Value(network_technology()));
    dictionary->SetKey(shill::kActivationStateProperty,
                       base::Value(activation_state()));
    dictionary->SetKey(shill::kRoamingStateProperty, base::Value(roaming()));
    dictionary->SetKey(shill::kOutOfCreditsProperty,
                       base::Value(cellular_out_of_credits()));
  }
}

void NetworkState::IPConfigPropertiesChanged(const base::Value& properties) {
  if (properties.DictEmpty()) {
    ipv4_config_.reset();
    return;
  }
  ipv4_config_ = std::make_unique<base::Value>(properties.Clone());
}

std::string NetworkState::GetIpAddress() const {
  return GetStringFromDictionary(ipv4_config_.get(), shill::kAddressProperty);
}

std::string NetworkState::GetGateway() const {
  return GetStringFromDictionary(ipv4_config_.get(), shill::kGatewayProperty);
}

GURL NetworkState::GetWebProxyAutoDiscoveryUrl() const {
  std::string url = GetStringFromDictionary(
      ipv4_config_.get(), shill::kWebProxyAutoDiscoveryUrlProperty);
  if (url.empty())
    return GURL();
  GURL gurl(url);
  if (!gurl.is_valid()) {
    NET_LOG(ERROR) << "Invalid WebProxyAutoDiscoveryUrl: " << path() << ": "
                   << url;
    return GURL();
  }
  return gurl;
}

void NetworkState::SetCaptivePortalProvider(const std::string& id,
                                            const std::string& name) {
  if (id.empty()) {
    captive_portal_provider_ = nullptr;
    return;
  }
  if (!captive_portal_provider_)
    captive_portal_provider_ = std::make_unique<CaptivePortalProviderInfo>();
  captive_portal_provider_->id = id;
  captive_portal_provider_->name = name;
}

std::string NetworkState::GetVpnProviderType() const {
  return vpn_provider_ ? vpn_provider_->type : std::string();
}

bool NetworkState::RequiresActivation() const {
  return type() == shill::kTypeCellular &&
         activation_state() != shill::kActivationStateActivated &&
         activation_state() != shill::kActivationStateUnknown;
}

bool NetworkState::SecurityRequiresPassphraseOnly() const {
  return type() == shill::kTypeWifi &&
         (security_class() == shill::kSecurityPsk ||
          security_class() == shill::kSecurityWep);
}

std::string NetworkState::connection_state() const {
  if (!visible())
    return shill::kStateDisconnect;
  return connection_state_;
}

void NetworkState::set_connection_state(const std::string connection_state) {
  last_connection_state_ = connection_state_;
  connection_state_ = connection_state;
}

bool NetworkState::IsManagedByPolicy() const {
  return onc_source_ == ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY ||
         onc_source_ == ::onc::ONCSource::ONC_SOURCE_USER_POLICY;
}

bool NetworkState::IsUsingMobileData() const {
  return type() == shill::kTypeCellular || type() == chromeos::kTypeTether ||
         tethering_state() == shill::kTetheringConfirmedState;
}

bool NetworkState::IsDynamicWep() const {
  return security_class_ == shill::kSecurityWep &&
         eap_key_mgmt_ == shill::kKeyManagementIEEE8021X;
}

bool NetworkState::IsConnectedState() const {
  return visible() && StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return visible() && StateIsConnecting(connection_state_);
}

bool NetworkState::IsConnectingOrConnected() const {
  return visible() && (StateIsConnecting(connection_state_) ||
                       StateIsConnected(connection_state_));
}

bool NetworkState::IsReconnecting() const {
  return visible() && StateIsConnecting(connection_state_) &&
         StateIsConnected(last_connection_state_);
}

bool NetworkState::IsInProfile() const {
  // kTypeEthernetEap is always saved. We need this check because it does
  // not show up in the visible list, but its properties may not be available
  // when it first shows up in ServiceCompleteList. See crbug.com/355117.
  return !profile_path_.empty() || type() == shill::kTypeEthernetEap;
}

bool NetworkState::IsNonProfileType() const {
  return type() == kTypeTether || IsDefaultCellular();
}

bool NetworkState::IsPrivate() const {
  return !profile_path_.empty() &&
         profile_path_ != NetworkProfileHandler::GetSharedProfilePath();
}

bool NetworkState::IsDefaultCellular() const {
  return type() == shill::kTypeCellular &&
         path() == kDefaultCellularNetworkPath;
}

std::string NetworkState::GetHexSsid() const {
  return base::HexEncode(raw_ssid().data(), raw_ssid().size());
}

std::string NetworkState::GetDnsServersAsString() const {
  const base::Value* listv =
      ipv4_config_ ? ipv4_config_->FindKey(shill::kNameServersProperty)
                   : nullptr;
  if (!listv)
    return std::string();
  std::string result;
  for (const auto& v : listv->GetList()) {
    if (!result.empty())
      result += ",";
    result += v.GetString();
  }
  return result;
}

std::string NetworkState::GetNetmask() const {
  const base::Value* v =
      ipv4_config_ ? ipv4_config_->FindKey(shill::kPrefixlenProperty) : nullptr;
  int prefixlen = v ? v->GetInt() : -1;
  return network_util::PrefixLengthToNetmask(prefixlen);
}

std::string NetworkState::GetSpecifier() const {
  if (!update_received()) {
    NET_LOG(ERROR) << "GetSpecifier called before update: " << path();
    return std::string();
  }
  if (type() == shill::kTypeWifi)
    return name() + "_" + security_class_;
  if (type() != shill::kTypeCellular && !name().empty())
    return name();
  return type();  // For unnamed networks, i.e. Ethernet and Cellular.
}

void NetworkState::SetGuid(const std::string& guid) {
  guid_ = guid;
}

std::string NetworkState::GetErrorState() const {
  if (ErrorIsValid(error()))
    return error();
  return last_error();
}

// static
bool NetworkState::StateIsConnected(const std::string& connection_state) {
  return (connection_state == shill::kStateReady ||
          connection_state == shill::kStateOnline ||
          connection_state == shill::kStatePortal);
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == shill::kStateAssociation ||
          connection_state == shill::kStateConfiguration ||
          connection_state == shill::kStateCarrier);
}

// static
bool NetworkState::NetworkStateIsCaptivePortal(
    const base::Value& shill_properties) {
  return IsCaptivePortalState(shill_properties, false /* log */);
}

// static
bool NetworkState::ErrorIsValid(const std::string& error) {
  // Shill uses "Unknown" to indicate an unset or cleared error state.
  return !error.empty() && error != kErrorUnknown;
}

// static
std::unique_ptr<NetworkState> NetworkState::CreateDefaultCellular(
    const std::string& device_path) {
  auto new_state = std::make_unique<NetworkState>(kDefaultCellularNetworkPath);
  new_state->set_type(shill::kTypeCellular);
  new_state->set_update_received();
  new_state->set_visible(true);
  new_state->device_path_ = device_path;
  return new_state;
}

// Private methods.

bool NetworkState::UpdateName(const base::Value& properties) {
  std::string updated_name =
      shill_property_util::GetNameFromProperties(path(), properties);
  if (updated_name != name()) {
    set_name(updated_name);
    return true;
  }
  return false;
}

void NetworkState::SetVpnProvider(const std::string& id,
                                  const std::string& type) {
  // |type| is required but |id| is only set for ThirdParty and Arc VPNs.
  if (type.empty()) {
    vpn_provider_ = nullptr;
    return;
  }
  if (!vpn_provider_)
    vpn_provider_ = std::make_unique<VpnProviderInfo>();
  vpn_provider_->id = id;
  vpn_provider_->type = type;
}

}  // namespace chromeos
