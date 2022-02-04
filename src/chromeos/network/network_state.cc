// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/shill_property_util.h"
#include "chromeos/network/tether_constants.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "net/http/http_status_code.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

// TODO(tbarzic): Add payment portal method values to shill/dbus-constants.
constexpr char kPaymentPortalMethodPost[] = "POST";

// TODO(b/169939319): Use shill constant once it lands.
const char kPortalDetectionFailedStatusCodeProperty[] =
    "PortalDetectionFailedStatusCode";

// |dict| may be an empty value, in which case return an empty string.
std::string GetStringFromDictionary(const base::Value& dict, const char* key) {
  const std::string* stringp =
      dict.is_none() ? nullptr : dict.FindStringKey(key);
  return stringp ? *stringp : std::string();
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
    int signal_strength = signal_strength_;
    if (!GetIntegerValue(key, value, &signal_strength))
      return false;
    if (signal_strength_ > 0 && abs(signal_strength - signal_strength_) <
                                    kSignalStrengthChangeThreshold) {
      return false;
    }
    signal_strength_ = signal_strength;
    return true;
  } else if (key == shill::kStateProperty) {
    std::string connection_state;
    if (!GetStringValue(key, value, &connection_state))
      return false;
    SetConnectionState(connection_state);
    return true;
  } else if (key == shill::kVisibleProperty) {
    return GetBooleanValue(key, value, &visible_);
  } else if (key == shill::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == shill::kErrorProperty) {
    std::string error;
    if (!GetStringValue(key, value, &error))
      return false;
    if (ErrorIsValid(error))
      last_error_ = error;
    return true;
  } else if (key == shill::kWifiFrequency) {
    return GetIntegerValue(key, value, &frequency_);
  } else if (key == shill::kActivationTypeProperty) {
    return GetStringValue(key, value, &activation_type_);
  } else if (key == shill::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == shill::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == shill::kCellularAllowRoamingProperty) {
    return GetBooleanValue(key, value, &allow_roaming_);
  } else if (key == shill::kPaymentPortalProperty) {
    if (!value.is_dict())
      return false;
    const base::Value* portal_url_value = value.FindKeyOfType(
        shill::kPaymentPortalURL, base::Value::Type::STRING);
    if (!portal_url_value)
      return false;
    payment_url_ = portal_url_value->GetString();
    // If payment portal uses post method, set up post data.
    const base::Value* portal_method_value = value.FindKeyOfType(
        shill::kPaymentPortalMethod, base::Value::Type::STRING);
    const base::Value* portal_post_data_value = value.FindKeyOfType(
        shill::kPaymentPortalPostData, base::Value::Type::STRING);
    if (portal_method_value &&
        portal_method_value->GetString() == kPaymentPortalMethodPost &&
        portal_post_data_value) {
      payment_post_data_ = portal_post_data_value->GetString();
    }
    return true;
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
  } else if (key == shill::kWifiHiddenSsid) {
    return GetBooleanValue(key, value, &hidden_ssid_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  } else if (key == shill::kIccidProperty) {
    return GetStringValue(key, value, &iccid_);
  } else if (key == shill::kEidProperty) {
    return GetStringValue(key, value, &eid_);
  } else if (key == shill::kProxyConfigProperty) {
    const std::string* proxy_config_str = value.GetIfString();
    if (!proxy_config_str) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }

    if ((*proxy_config_str).empty()) {
      proxy_config_ = base::Value();
      return true;
    }
    base::Value proxy_config = onc::ReadDictionaryFromJson(*proxy_config_str);
    if (!proxy_config.is_dict()) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      proxy_config_ = base::Value();
    } else {
      proxy_config_ = std::move(proxy_config);
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
  } else if (key == shill::kUIDataProperty) {
    std::unique_ptr<NetworkUIData> ui_data =
        chromeos::shill_property_util::GetUIDataFromValue(value);
    if (!ui_data)
      return false;
    onc_source_ = ui_data->onc_source();
    return true;
  } else if (key == shill::kProbeUrlProperty) {
    std::string probe_url_string;
    if (!GetStringValue(key, value, &probe_url_string))
      return false;
    probe_url_ = GURL(probe_url_string);
    return true;
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(const base::Value& properties) {
  NET_LOG(EVENT) << "InitialPropertiesReceived: " << NetworkId(this)
                 << " State: " << connection_state_ << " Visible: " << visible_;
  if (!properties.FindKey(shill::kTypeProperty)) {
    NET_LOG(ERROR) << "NetworkState has no type: " << NetworkId(this);
    return false;
  }

  // By convention, all visible WiFi networks have a SignalStrength > 0.
  if (type() == shill::kTypeWifi && visible() && signal_strength_ <= 0) {
    signal_strength_ = 1;
  }

  // Any change to connection state or portal properties will trigger a complete
  // property update, so we update captive portal state here.
  UpdateCaptivePortalState(properties);

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
    dictionary->SetKey(kTetherCarrier, base::Value(tether_carrier()));
    dictionary->SetKey(kTetherHasConnectedToHost,
                       base::Value(tether_has_connected_to_host()));
    dictionary->SetKey(kTetherSignalStrength, base::Value(signal_strength()));

    // All Tether networks are connectable.
    dictionary->SetKey(shill::kConnectableProperty, base::Value(connectable()));

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
  }

  // Mobile properties
  if (NetworkTypePattern::Mobile().MatchesType(type())) {
    dictionary->SetKey(shill::kNetworkTechnologyProperty,
                       base::Value(network_technology()));
    dictionary->SetKey(shill::kActivationStateProperty,
                       base::Value(activation_state()));
    dictionary->SetKey(shill::kRoamingStateProperty, base::Value(roaming_));
    dictionary->SetKey(shill::kOutOfCreditsProperty,
                       base::Value(cellular_out_of_credits()));
  }

  // Cellular properties
  if (NetworkTypePattern::Cellular().MatchesType(type())) {
    dictionary->SetKey(shill::kIccidProperty, base::Value(iccid()));
    dictionary->SetKey(shill::kEidProperty, base::Value(eid()));
  }
}

bool NetworkState::IsActive() const {
  return IsConnectingOrConnected() ||
         activation_state() == shill::kActivationStateActivating;
}

void NetworkState::IPConfigPropertiesChanged(const base::Value& properties) {
  if (properties.DictEmpty()) {
    ipv4_config_ = base::Value();
    return;
  }
  ipv4_config_ = properties.Clone();
}

std::string NetworkState::GetIpAddress() const {
  return GetStringFromDictionary(ipv4_config_, shill::kAddressProperty);
}

std::string NetworkState::GetGateway() const {
  return GetStringFromDictionary(ipv4_config_, shill::kGatewayProperty);
}

GURL NetworkState::GetWebProxyAutoDiscoveryUrl() const {
  std::string url = GetStringFromDictionary(
      ipv4_config_, shill::kWebProxyAutoDiscoveryUrlProperty);
  if (url.empty())
    return GURL();
  GURL gurl(url);
  if (!gurl.is_valid()) {
    NET_LOG(ERROR) << "Invalid WebProxyAutoDiscoveryUrl: " << NetworkId(this)
                   << ": " << url;
    return GURL();
  }
  return gurl;
}

std::string NetworkState::GetVpnProviderType() const {
  return vpn_provider_ ? vpn_provider_->type : std::string();
}

bool NetworkState::RequiresActivation() const {
  return type() == shill::kTypeCellular &&
         (activation_state() == shill::kActivationStateNotActivated ||
          activation_state() == shill::kActivationStatePartiallyActivated);
}

bool NetworkState::SecurityRequiresPassphraseOnly() const {
  return type() == shill::kTypeWifi &&
         (security_class_ == shill::kSecurityPsk ||
          security_class_ == shill::kSecurityWep);
}

const std::string& NetworkState::GetError() const {
  return last_error_;
}

void NetworkState::ClearError() {
  last_error_.clear();
}

std::string NetworkState::connection_state() const {
  if (!visible())
    return shill::kStateIdle;
  DCHECK(connection_state_ == shill::kStateIdle ||
         connection_state_ == shill::kStateAssociation ||
         connection_state_ == shill::kStateConfiguration ||
         connection_state_ == shill::kStateReady ||
         connection_state_ == shill::kStatePortal ||
         connection_state_ == shill::kStateNoConnectivity ||
         connection_state_ == shill::kStateRedirectFound ||
         connection_state_ == shill::kStatePortalSuspected ||
         // TODO(https://crbug.com/552190): Remove kStateOffline from this list
         // when occurrences in chromium code have been eliminated.
         connection_state_ == shill::kStateOffline ||
         connection_state_ == shill::kStateOnline ||
         connection_state_ == shill::kStateFailure ||
         connection_state_ == shill::kStateDisconnect ||
         // TODO(https://crbug.com/552190): Remove kStateActivationFailure from
         // this list when occurrences in chromium code have been eliminated.
         connection_state_ == shill::kStateActivationFailure ||
         // TODO(https://crbug.com/552190): Empty should not be a valid state,
         // but e.g. new tether NetworkStates and unit tests use it currently.
         connection_state_.empty());

  return connection_state_;
}

void NetworkState::SetConnectionState(const std::string& connection_state) {
  if (connection_state == connection_state_)
    return;
  last_connection_state_ = connection_state_;
  connection_state_ = connection_state;
  if (StateIsConnected(connection_state_) ||
      StateIsConnecting(last_connection_state_)) {
    // If connected or previously connecting, clear |connect_requested_|.
    connect_requested_ = false;
  } else if (StateIsConnected(last_connection_state_) &&
             StateIsConnecting(connection_state_)) {
    // If transitioning from a connected state to a connecting state, set
    // |connect_requested_| so that the UI knows the connecting state is
    // important (i.e. not a normal auto connect).
    connect_requested_ = true;
  }
}

bool NetworkState::IsManagedByPolicy() const {
  return onc_source_ == ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY ||
         onc_source_ == ::onc::ONCSource::ONC_SOURCE_USER_POLICY;
}

bool NetworkState::IndicateRoaming() const {
  return type() == shill::kTypeCellular &&
         roaming_ == shill::kRoamingStateRoaming && !provider_requires_roaming_;
}

bool NetworkState::IsDynamicWep() const {
  return security_class_ == shill::kSecurityWep &&
         eap_key_mgmt_ == shill::kKeyManagementIEEE8021X;
}

bool NetworkState::IsConnectedState() const {
  return visible() && StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return visible() &&
         (connect_requested_ || StateIsConnecting(connection_state_));
}

bool NetworkState::IsConnectingOrConnected() const {
  return visible() &&
         (connect_requested_ || StateIsConnecting(connection_state_) ||
          StateIsConnected(connection_state_));
}

bool NetworkState::IsOnline() const {
  return connection_state() == shill::kStateOnline;
}

bool NetworkState::IsInProfile() const {
  // kTypeEthernetEap is always saved. We need this check because it does
  // not show up in the visible list, but its properties may not be available
  // when it first shows up in ServiceCompleteList. See crbug.com/355117.
  return !profile_path_.empty() || type() == shill::kTypeEthernetEap;
}

bool NetworkState::IsNonProfileType() const {
  return type() == kTypeTether || IsNonShillCellularNetwork();
}

bool NetworkState::IsPrivate() const {
  return !profile_path_.empty() &&
         profile_path_ != NetworkProfileHandler::GetSharedProfilePath();
}

bool NetworkState::IsNonShillCellularNetwork() const {
  return type() == shill::kTypeCellular && IsStubCellularServicePath(path());
}

bool NetworkState::IsShillCaptivePortal() const {
  switch (portal_state_) {
    case PortalState::kUnknown:
    case PortalState::kOnline:
      return false;
    case PortalState::kPortalSuspected:
    case PortalState::kPortal:
    case PortalState::kProxyAuthRequired:
    case PortalState::kNoInternet:
      return true;
  }
  NOTREACHED();
  return false;
}

bool NetworkState::IsCaptivePortal() const {
  return is_chrome_captive_portal_ || IsShillCaptivePortal();
}

bool NetworkState::IsSecure() const {
  return !security_class_.empty() && security_class_ != shill::kSecurityNone;
}

std::string NetworkState::GetHexSsid() const {
  return base::HexEncode(raw_ssid().data(), raw_ssid().size());
}

std::string NetworkState::GetDnsServersAsString() const {
  const base::Value* listv =
      ipv4_config_.is_none()
          ? nullptr
          : ipv4_config_.FindListKey(shill::kNameServersProperty);
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
  int prefixlen =
      ipv4_config_.FindIntKey(shill::kPrefixlenProperty).value_or(-1);
  return network_util::PrefixLengthToNetmask(prefixlen);
}

std::string NetworkState::GetSpecifier() const {
  if (!update_received()) {
    NET_LOG(ERROR) << "GetSpecifier called before update: " << NetworkId(this);
    return std::string();
  }
  if (type() == shill::kTypeWifi)
    return name() + "_" + security_class_;
  if (type() == shill::kTypeCellular && !iccid().empty())
    return iccid();
  if (!name().empty())
    return type() + "_" + name();
  return type();  // For unnamed networks, i.e. Ethernet.
}

void NetworkState::SetGuid(const std::string& guid) {
  guid_ = guid;
}

network_config::mojom::ActivationStateType
NetworkState::GetMojoActivationState() const {
  using network_config::mojom::ActivationStateType;
  if (activation_state_.empty())
    return ActivationStateType::kUnknown;
  if (activation_state_ == shill::kActivationStateActivated)
    return ActivationStateType::kActivated;
  if (activation_state_ == shill::kActivationStateActivating)
    return ActivationStateType::kActivating;
  if (activation_state_ == shill::kActivationStateNotActivated)
    return ActivationStateType::kNotActivated;
  if (activation_state_ == shill::kActivationStatePartiallyActivated)
    return ActivationStateType::kPartiallyActivated;
  NET_LOG(ERROR) << "Unexpected shill activation state: " << activation_state_;
  return ActivationStateType::kUnknown;
}

network_config::mojom::SecurityType NetworkState::GetMojoSecurity() const {
  using network_config::mojom::SecurityType;
  if (!IsSecure())
    return SecurityType::kNone;
  if (IsDynamicWep())
    return SecurityType::kWep8021x;

  if (security_class_ == shill::kSecurityWep)
    return SecurityType::kWepPsk;
  if (security_class_ == shill::kSecurityPsk)
    return SecurityType::kWpaPsk;
  if (security_class_ == shill::kSecurity8021x)
    return SecurityType::kWpaEap;
  NET_LOG(ERROR) << "Unsupported shill security class: " << security_class_;
  return SecurityType::kNone;
}

NetworkState::NetworkTechnologyType NetworkState::GetNetworkTechnologyType()
    const {
  const std::string& network_type = type();
  if (network_type == shill::kTypeCellular)
    return NetworkTechnologyType::kCellular;
  if (network_type == shill::kTypeEthernet)
    return NetworkTechnologyType::kEthernet;
  if (network_type == shill::kTypeEthernetEap)
    return NetworkTechnologyType::kEthernet;
  if (network_type == kTypeTether)
    return NetworkTechnologyType::kTether;
  if (network_type == shill::kTypeVPN)
    return NetworkTechnologyType::kVPN;
  if (network_type == shill::kTypeWifi)
    return NetworkTechnologyType::kWiFi;
  NOTREACHED() << "Unknown network type: " << network_type;
  return NetworkTechnologyType::kUnknown;
}

// static
bool NetworkState::StateIsConnected(const std::string& connection_state) {
  return (connection_state == shill::kStateReady ||
          connection_state == shill::kStateOnline ||
          StateIsPortalled(connection_state));
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == shill::kStateAssociation ||
          connection_state == shill::kStateConfiguration);
}

// static
bool NetworkState::StateIsPortalled(const std::string& connection_state) {
  return (connection_state == shill::kStatePortal ||
          connection_state == shill::kStateNoConnectivity ||
          connection_state == shill::kStateRedirectFound ||
          connection_state == shill::kStatePortalSuspected);
}

// static
bool NetworkState::ErrorIsValid(const std::string& error) {
  return !error.empty() && error != shill::kErrorNoFailure;
}

// static
std::unique_ptr<NetworkState> NetworkState::CreateNonShillCellularNetwork(
    const std::string& iccid,
    const std::string& eid,
    const std::string& guid,
    const DeviceState* cellular_device) {
  std::string path = GenerateStubCellularServicePath(iccid);
  auto new_state = std::make_unique<NetworkState>(path);
  new_state->set_type(shill::kTypeCellular);
  new_state->set_update_received();
  new_state->set_visible(true);
  new_state->device_path_ = cellular_device->path();
  new_state->iccid_ = iccid;
  new_state->eid_ = eid;
  new_state->guid_ = guid;
  new_state->activation_state_ = shill::kActivationStateActivated;
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

void NetworkState::UpdateCaptivePortalState(const base::Value& properties) {
  int status_code =
      properties.FindIntKey(kPortalDetectionFailedStatusCodeProperty)
          .value_or(0);
  if (connection_state_ == shill::kStateNoConnectivity) {
    portal_state_ = PortalState::kNoInternet;
  } else if (connection_state_ == shill::kStatePortal ||
             connection_state_ == shill::kStateRedirectFound) {
    portal_state_ = status_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED
                        ? PortalState::kProxyAuthRequired
                        : PortalState::kPortal;
  } else if (connection_state_ == shill::kStatePortalSuspected) {
    portal_state_ = PortalState::kPortalSuspected;
  } else {
    portal_state_ = PortalState::kOnline;
  }

  UMA_HISTOGRAM_ENUMERATION("CaptivePortal.NetworkStateResult", portal_state_);
  if (portal_state_ != PortalState::kOnline) {
    portal_status_code_ = status_code;
    NET_LOG(EVENT) << "Network is in captive portal state: " << NetworkId(this)
                   << " status_code=" << portal_status_code_;
    base::UmaHistogramSparse("CaptivePortal.NetworkStateStatusCode",
                             std::abs(portal_status_code_));
  } else {
    portal_status_code_ = 0;
  }
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
