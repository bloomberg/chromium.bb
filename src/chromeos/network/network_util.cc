// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_util.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/onc/onc_translator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

WifiAccessPoint::WifiAccessPoint()
    : signal_strength(0),
      signal_to_noise(0),
      channel(0) {
}

WifiAccessPoint::WifiAccessPoint(const WifiAccessPoint& other) = default;

WifiAccessPoint::~WifiAccessPoint() = default;

CellTower::CellTower() = default;

CellTower::CellTower(const CellTower& other) = default;

CellTower::~CellTower() = default;

CellularScanResult::CellularScanResult() = default;

CellularScanResult::CellularScanResult(const CellularScanResult& other) =
    default;

CellularScanResult::~CellularScanResult() = default;

namespace network_util {

std::string PrefixLengthToNetmask(int32_t prefix_length) {
  std::string netmask;
  // Return the empty string for invalid inputs.
  if (prefix_length < 0 || prefix_length > 32)
    return netmask;
  for (int i = 0; i < 4; i++) {
    int remainder = 8;
    if (prefix_length >= 8) {
      prefix_length -= 8;
    } else {
      remainder = prefix_length;
      prefix_length = 0;
    }
    if (i > 0)
      netmask += ".";
    int value = remainder == 0 ? 0 :
        ((2L << (remainder - 1)) - 1) << (8 - remainder);
    netmask += base::NumberToString(value);
  }
  return netmask;
}

int32_t NetmaskToPrefixLength(const std::string& netmask) {
  int count = 0;
  int prefix_length = 0;
  base::StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4)
      return -1;

    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefix_length / 8 != count) {
      if (token != "0")
        return -1;
    } else if (token == "255") {
      prefix_length += 8;
    } else if (token == "254") {
      prefix_length += 7;
    } else if (token == "252") {
      prefix_length += 6;
    } else if (token == "248") {
      prefix_length += 5;
    } else if (token == "240") {
      prefix_length += 4;
    } else if (token == "224") {
      prefix_length += 3;
    } else if (token == "192") {
      prefix_length += 2;
    } else if (token == "128") {
      prefix_length += 1;
    } else if (token == "0") {
      prefix_length += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefix_length;
}

std::string FormattedMacAddress(const std::string& shill_mac_address) {
  if (shill_mac_address.size() % 2 != 0)
    return shill_mac_address;
  std::string result;
  for (size_t i = 0; i < shill_mac_address.size(); ++i) {
    if ((i != 0) && (i % 2 == 0))
      result.push_back(':');
    result.push_back(base::ToUpperASCII(shill_mac_address[i]));
  }
  return result;
}

bool ParseCellularScanResults(const base::ListValue& list,
                              std::vector<CellularScanResult>* scan_results) {
  scan_results->clear();
  scan_results->reserve(list.GetSize());
  for (const auto& value : list) {
    const base::DictionaryValue* dict;
    if (!value.GetAsDictionary(&dict))
      return false;
    CellularScanResult scan_result;
    // If the network id property is not present then this network cannot be
    // connected to so don't include it in the results.
    if (!dict->GetStringWithoutPathExpansion(shill::kNetworkIdProperty,
                                             &scan_result.network_id))
      continue;
    dict->GetStringWithoutPathExpansion(shill::kStatusProperty,
                                        &scan_result.status);
    dict->GetStringWithoutPathExpansion(shill::kLongNameProperty,
                                        &scan_result.long_name);
    dict->GetStringWithoutPathExpansion(shill::kShortNameProperty,
                                        &scan_result.short_name);
    dict->GetStringWithoutPathExpansion(shill::kTechnologyProperty,
                                        &scan_result.technology);
    scan_results->push_back(scan_result);
  }
  return true;
}

std::unique_ptr<base::DictionaryValue> TranslateNetworkStateToONC(
    const NetworkState* network) {
  // Get the properties from the NetworkState.
  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      new base::DictionaryValue);
  network->GetStateProperties(shill_dictionary.get());

  // Get any Device properties required to translate state.
  if (NetworkTypePattern::Cellular().MatchesType(network->type())) {
    const DeviceState* device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceState(
            network->device_path());
    if (device) {
      base::DictionaryValue device_dict;
      // We need to set Device.Cellular.ProviderRequiresRoaming so that
      // Cellular.RoamingState can be set correctly for badging network icons.
      device_dict.SetKey(shill::kProviderRequiresRoamingProperty,
                         base::Value(device->provider_requires_roaming()));
      // Scanning is also used in the UI when displaying a list of networks.
      device_dict.SetKey(shill::kScanningProperty,
                         base::Value(device->scanning()));
      shill_dictionary->SetKey(shill::kDeviceProperty, std::move(device_dict));
    }
  }

  // NetworkState is always associated with the primary user profile, regardless
  // of what profile is associated with the page that calls this method. We do
  // not expose any sensitive properties in the resulting dictionary, it is
  // only used to show connection state and icons.
  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->FindPolicyByGUID(user_id_hash, network->guid(), &onc_source);

  std::unique_ptr<base::DictionaryValue> onc_dictionary =
      TranslateShillServiceToONCPart(*shill_dictionary, onc_source,
                                     &onc::kNetworkWithStateSignature, network);
  return onc_dictionary;
}

std::unique_ptr<base::ListValue> TranslateNetworkListToONC(
    NetworkTypePattern pattern,
    bool configured_only,
    bool visible_only,
    int limit) {
  NetworkStateHandler::NetworkStateList network_states;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      pattern, configured_only, visible_only, limit, &network_states);

  std::unique_ptr<base::ListValue> network_properties_list(new base::ListValue);
  for (const NetworkState* state : network_states) {
    std::unique_ptr<base::DictionaryValue> onc_dictionary =
        TranslateNetworkStateToONC(state);
    network_properties_list->Append(std::move(onc_dictionary));
  }
  return network_properties_list;
}

std::string TranslateONCTypeToShill(const std::string& onc_type) {
  if (onc_type == ::onc::network_type::kEthernet)
    return shill::kTypeEthernet;
  std::string shill_type;
  onc::TranslateStringToShill(onc::kNetworkTypeTable, onc_type, &shill_type);
  return shill_type;
}

std::string TranslateONCSecurityToShill(const std::string& onc_security) {
  std::string shill_security;
  onc::TranslateStringToShill(onc::kWiFiSecurityTable, onc_security,
                              &shill_security);
  return shill_security;
}

std::string TranslateShillTypeToONC(const std::string& shill_type) {
  if (shill_type == shill::kTypeEthernet)
    return ::onc::network_type::kEthernet;
  std::string onc_type;
  onc::TranslateStringToONC(onc::kNetworkTypeTable, shill_type, &onc_type);
  return onc_type;
}

}  // namespace network_util
}  // namespace chromeos
