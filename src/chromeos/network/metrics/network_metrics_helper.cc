// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/network_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/network/metrics/connection_results.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char kNetworkMetricsPrefix[] = "Network.Ash.";
const char kAllConnectionResultSuffix[] = ".ConnectionResult.All";
const char kUserInitiatedConnectionResultSuffix[] =
    ".ConnectionResult.UserInitiated";
const char kDisconnectionsWithoutUserActionSuffix[] =
    ".DisconnectionsWithoutUserAction";

const char kEnableTechnologyResultSuffix[] = ".EnabledState.Enable.Result";
const char kDisableTechnologyResultSuffix[] = ".EnabledState.Disable.Result";

const char kCellular[] = "Cellular";
const char kCellularESim[] = "Cellular.ESim";
const char kCellularPSim[] = "Cellular.PSim";

const char kEthernet[] = "Ethernet";
const char kEthernetEap[] = "Ethernet.Eap";
const char kEthernetNoEap[] = "Ethernet.NoEap";

const char kTether[] = "Tether";

const char kVPN[] = "VPN";
const char kVPNBuiltIn[] = "VPN.TypeBuiltIn";
const char kVPNThirdParty[] = "VPN.TypeThirdParty";

const char kWifi[] = "WiFi";
const char kWifiOpen[] = "WiFi.SecurityOpen";
const char kWifiPasswordProtected[] = "WiFi.SecurityPasswordProtected";

chromeos::NetworkStateHandler* GetNetworkStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

const absl::optional<const std::string> GetTechnologyTypeSuffix(
    const std::string& technology) {
  // Note that Tether is a fake technology that does not correspond to shill
  // technology type.
  if (technology == shill::kTypeWifi)
    return kWifi;
  else if (technology == shill::kTypeEthernet)
    return kEthernet;
  else if (technology == shill::kTypeCellular)
    return kCellular;
  else if (technology == shill::kTypeVPN)
    return kVPN;
  return absl::nullopt;
}

const std::vector<std::string> GetCellularNetworkTypeHistogams(
    const NetworkState* network_state) {
  std::vector<std::string> cellular_histograms{kCellular};

  if (network_state->eid().empty())
    cellular_histograms.emplace_back(kCellularPSim);
  else
    cellular_histograms.emplace_back(kCellularESim);
  return cellular_histograms;
}

const std::vector<std::string> GetEthernetNetworkTypeHistograms(
    const NetworkState* network_state) {
  std::vector<std::string> ethernet_histograms{kEthernet};
  if (GetNetworkStateHandler()->GetEAPForEthernet(network_state->path(),
                                                  /*connected_only=*/true)) {
    ethernet_histograms.emplace_back(kEthernetEap);
  } else {
    ethernet_histograms.emplace_back(kEthernetNoEap);
  }

  return ethernet_histograms;
}

const std::vector<std::string> GetWifiNetworkTypeHistograms(
    const NetworkState* network_state) {
  std::vector<std::string> wifi_histograms{kWifi};

  if (network_state->GetMojoSecurity() ==
      network_config::mojom::SecurityType::kNone) {
    wifi_histograms.emplace_back(kWifiOpen);
  } else {
    wifi_histograms.emplace_back(kWifiPasswordProtected);
  }

  return wifi_histograms;
}

const std::vector<std::string> GetTetherNetworkTypeHistograms(
    const NetworkState* network_state) {
  return {kTether};
}

const std::vector<std::string> GetVpnNetworkTypeHistograms(
    const NetworkState* network_state) {
  const std::string& vpn_provider_type = network_state->GetVpnProviderType();

  if (vpn_provider_type.empty())
    return {};

  std::vector<std::string> vpn_histograms{kVPN};

  if (vpn_provider_type == shill::kProviderThirdPartyVpn ||
      vpn_provider_type == shill::kProviderArcVpn) {
    vpn_histograms.emplace_back(kVPNThirdParty);
  } else if (vpn_provider_type == shill::kProviderL2tpIpsec ||
             vpn_provider_type == shill::kProviderOpenVpn ||
             vpn_provider_type == shill::kProviderWireGuard) {
    vpn_histograms.emplace_back(kVPNBuiltIn);
  } else {
    NOTREACHED();
  }
  return vpn_histograms;
}

const std::vector<std::string> GetNetworkTypeHistogramNames(
    const NetworkState* network_state) {
  switch (network_state->GetNetworkTechnologyType()) {
    case NetworkState::NetworkTechnologyType::kCellular:
      return GetCellularNetworkTypeHistogams(network_state);
    case NetworkState::NetworkTechnologyType::kEthernet:
      return GetEthernetNetworkTypeHistograms(network_state);
    case NetworkState::NetworkTechnologyType::kWiFi:
      return GetWifiNetworkTypeHistograms(network_state);
    case NetworkState::NetworkTechnologyType::kTether:
      return GetTetherNetworkTypeHistograms(network_state);
    case NetworkState::NetworkTechnologyType::kVPN:
      return GetVpnNetworkTypeHistograms(network_state);

    // There should not be connections requests for kEthernetEap type service.
    // kEthernetEap exists only to store auth details for ethernet.
    case NetworkState::NetworkTechnologyType::kEthernetEap:
      [[fallthrough]];
    case NetworkState::NetworkTechnologyType::kUnknown:
      [[fallthrough]];
    default:
      return {};
  }
}

}  // namespace

// static
void NetworkMetricsHelper::LogAllConnectionResult(
    const std::string& guid,
    const absl::optional<std::string>& shill_error) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state)
    return;

  ShillConnectResult connect_result =
      shill_error ? ShillErrorToConnectResult(*shill_error)
                  : ShillConnectResult::kSuccess;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {kNetworkMetricsPrefix, network_type, kAllConnectionResultSuffix}),
        connect_result);
  }
}

// static
void NetworkMetricsHelper::LogUserInitiatedConnectionResult(
    const std::string& guid,
    const absl::optional<std::string>& network_connection_error) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state)
    return;

  UserInitiatedConnectResult connect_result =
      network_connection_error
          ? NetworkConnectionErrorToConnectResult(*network_connection_error)
          : UserInitiatedConnectResult::kSuccess;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(
        base::StrCat({kNetworkMetricsPrefix, network_type,
                      kUserInitiatedConnectionResultSuffix}),
        connect_result);
  }
}

// static
void NetworkMetricsHelper::LogConnectionStateResult(const std::string& guid,
                                                    ConnectionState status) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state)
    return;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(kNetworkMetricsPrefix + network_type +
                                      kDisconnectionsWithoutUserActionSuffix,
                                  status);
  }
}

void NetworkMetricsHelper::LogEnableTechnologyResult(
    const std::string& technology,
    bool success) {
  absl::optional<const std::string> suffix =
      GetTechnologyTypeSuffix(technology);

  if (!suffix)
    return;

  base::UmaHistogramBoolean(base::StrCat({kNetworkMetricsPrefix, *suffix,
                                          kEnableTechnologyResultSuffix}),
                            success);
}

// static
void NetworkMetricsHelper::LogDisableTechnologyResult(
    const std::string& technology,
    bool success) {
  absl::optional<const std::string> suffix =
      GetTechnologyTypeSuffix(technology);

  if (!suffix)
    return;

  base::UmaHistogramBoolean(base::StrCat({kNetworkMetricsPrefix, *suffix,
                                          kDisableTechnologyResultSuffix}),
                            success);
}

NetworkMetricsHelper::NetworkMetricsHelper() = default;

NetworkMetricsHelper::~NetworkMetricsHelper() = default;

}  // namespace chromeos
