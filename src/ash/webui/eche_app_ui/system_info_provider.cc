// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info_provider.h"

#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/webui/eche_app_ui/mojom/types_mojom_traits.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {
namespace eche_app {

const char kJsonDeviceNameKey[] = "device_name";
const char kJsonBoardNameKey[] = "board_name";
const char kJsonTabletModeKey[] = "tablet_mode";
const char kJsonWifiConnectionStateKey[] = "wifi_connection_state";

using chromeos::network_config::mojom::ConnectionStateType;
// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace network_config = ::chromeos::network_config;

const std::map<ConnectionStateType, const char*> CONNECTION_STATE_TYPE{
    {ConnectionStateType::kOnline, "online"},
    {ConnectionStateType::kConnected, "connected"},
    {ConnectionStateType::kPortal, "portal"},
    {ConnectionStateType::kConnecting, "connecting"},
    {ConnectionStateType::kNotConnected, "not_connected"}};

SystemInfoProvider::SystemInfoProvider(
    std::unique_ptr<SystemInfo> system_info,
    network_config::mojom::CrosNetworkConfig* cros_network_config)
    : system_info_(std::move(system_info)),
      cros_network_config_(cros_network_config),
      wifi_connection_state_(ConnectionStateType::kNotConnected) {
  // TODO(samchiu): The intention of null check was for unit test. Add a fake
  // ScreenBacklight object to remove null check.
  if (ScreenBacklight::Get())
    ScreenBacklight::Get()->AddObserver(this);
  TabletMode::Get()->AddObserver(this);
  cros_network_config_->AddObserver(
      cros_network_config_receiver_.BindNewPipeAndPassRemote());
  FetchWifiNetworkList();
}

SystemInfoProvider::~SystemInfoProvider() {
  // Ash may be released before us.
  if (ScreenBacklight::Get())
    ScreenBacklight::Get()->RemoveObserver(this);
  if (TabletMode::Get())
    TabletMode::Get()->RemoveObserver(this);
}

void SystemInfoProvider::GetSystemInfo(
    base::OnceCallback<void(const std::string&)> callback) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider GetSystemInfo";
  base::DictionaryValue json_dictionary;
  json_dictionary.SetString(kJsonDeviceNameKey, system_info_->GetDeviceName());
  json_dictionary.SetString(kJsonBoardNameKey, system_info_->GetBoardName());
  json_dictionary.SetBoolean(kJsonTabletModeKey,
                             TabletMode::Get()->InTabletMode());
  auto found_type = CONNECTION_STATE_TYPE.find(wifi_connection_state_);
  std::string connecton_state_string =
      found_type == CONNECTION_STATE_TYPE.end() ? "" : found_type->second;
  json_dictionary.SetString(kJsonWifiConnectionStateKey,
                            connecton_state_string);

  std::string json_message;
  base::JSONWriter::Write(json_dictionary, &json_message);
  std::move(callback).Run(json_message);
}

void SystemInfoProvider::SetSystemInfoObserver(
    mojo::PendingRemote<mojom::SystemInfoObserver> observer) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider SetSystemInfoObserver";
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
}

void SystemInfoProvider::Bind(
    mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {
  info_receiver_.reset();
  info_receiver_.Bind(std::move(receiver));
}

void SystemInfoProvider::OnScreenBacklightStateChanged(
    ash::ScreenBacklightState screen_state) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider OnScreenBacklightStateChanged";
  if (!observer_remote_.is_bound())
    return;

  observer_remote_->OnScreenBacklightStateChanged(screen_state);
}

void SystemInfoProvider::SetTabletModeChanged(bool enabled) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider SetTabletModeChanged";
  if (!observer_remote_.is_bound())
    return;

  PA_LOG(VERBOSE) << "OnReceivedTabletModeChanged:" << enabled;
  observer_remote_->OnReceivedTabletModeChanged(enabled);
}

// TabletModeObserver implementation:
void SystemInfoProvider::OnTabletModeStarted() {
  SetTabletModeChanged(true);
}

void SystemInfoProvider::OnTabletModeEnded() {
  SetTabletModeChanged(false);
}

// network_config::mojom::CrosNetworkConfigObserver implementation:
void SystemInfoProvider::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network) {
  FetchWifiNetworkList();
}

void SystemInfoProvider::FetchWifiNetworkList() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kVisible,
          network_config::mojom::NetworkType::kWiFi,
          network_config::mojom::kNoLimit),
      base::BindOnce(&SystemInfoProvider::OnWifiNetworkList,
                     base::Unretained(this)));
}

void SystemInfoProvider::OnWifiNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  using chromeos::network_config::mojom::NetworkType;

  for (const auto& network : networks) {
    if (network->type == NetworkType::kWiFi) {
      PA_LOG(VERBOSE) << "OnWifiNetworkList: " << network->connection_state;
      wifi_connection_state_ = network->connection_state;
      return;
    }
  }
}

}  // namespace eche_app
}  // namespace ash
