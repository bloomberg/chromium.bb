// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_

#include "ash/public/cpp/tablet_mode_observer.h"
#include "chromeos/components/eche_app_ui/mojom/eche_app.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace eche_app {

extern const char kJsonDeviceNameKey[];
extern const char kJsonBoardNameKey[];
extern const char kJsonTabletModeKey[];
extern const char kJsonWifiConnectionStateKey[];

class SystemInfo;

// Provides the system information likes board/device names for EcheApp and
// exposes the interface via mojoa.
class SystemInfoProvider
    : public mojom::SystemInfoProvider,
      public ash::ScreenBacklightObserver,
      public ash::TabletModeObserver,
      public network_config::mojom::CrosNetworkConfigObserver {
 public:
  explicit SystemInfoProvider(
      std::unique_ptr<SystemInfo> system_info,
      network_config::mojom::CrosNetworkConfig* cros_network_config);
  ~SystemInfoProvider() override;

  SystemInfoProvider(const SystemInfoProvider&) = delete;
  SystemInfoProvider& operator=(const SystemInfoProvider&) = delete;

  // mojom::SystemInfoProvider:
  void GetSystemInfo(
      base::OnceCallback<void(const std::string&)> callback) override;
  void SetSystemInfoObserver(
      mojo::PendingRemote<mojom::SystemInfoObserver> observer) override;

  void Bind(mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

 private:
  friend class SystemInfoProviderTest;

  // ash::ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ash::ScreenBacklightState screen_state) override;
  // ash:TabletModeObserver overrides.
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  void SetTabletModeChanged(bool enabled);

  // network_config::mojom::CrosNetworkConfigObserver overrides:
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks)
      override {}
  void OnDeviceStateListChanged() override {}
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;
  void OnNetworkStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

  void FetchWifiNetworkList();
  void OnWifiNetworkList(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks);

  mojo::Receiver<mojom::SystemInfoProvider> info_receiver_{this};
  mojo::Remote<mojom::SystemInfoObserver> observer_remote_;
  mojo::Receiver<network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_receiver_{this};
  std::unique_ptr<SystemInfo> system_info_;
  network_config::mojom::CrosNetworkConfig* cros_network_config_;
  network_config::mojom::ConnectionStateType wifi_connection_state_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_
