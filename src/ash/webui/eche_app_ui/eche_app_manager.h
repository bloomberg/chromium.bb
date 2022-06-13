// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_APP_MANAGER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_APP_MANAGER_H_

#include <stdint.h>
#include <memory>

#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"
#include "ash/webui/eche_app_ui/eche_notification_click_handler.h"
#include "ash/webui/eche_app_ui/eche_recent_app_click_handler.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace ash {
namespace eche_app {

class EcheConnector;
class EcheMessageReceiver;
class EcheNotificationGenerator;
class EchePresenceManager;
class EcheSignaler;
class EcheUidProvider;
class SystemInfo;
class SystemInfoProvider;
class AppsAccessManager;

// Implements the core logic of the EcheApp and exposes interfaces via its
// public API. Implemented as a KeyedService since it depends on other
// KeyedService instances.
class EcheAppManager : public KeyedService {
 public:
  EcheAppManager(PrefService* pref_service,
                 std::unique_ptr<SystemInfo> system_info,
                 phonehub::PhoneHubManager*,
                 device_sync::DeviceSyncClient*,
                 multidevice_setup::MultiDeviceSetupClient*,
                 secure_channel::SecureChannelClient*,
                 std::unique_ptr<secure_channel::PresenceMonitorClient>
                     presence_monitor_client,
                 LaunchAppHelper::LaunchEcheAppFunction,
                 LaunchAppHelper::CloseEcheAppFunction,
                 LaunchAppHelper::LaunchNotificationFunction);
  ~EcheAppManager() override;

  EcheAppManager(const EcheAppManager&) = delete;
  EcheAppManager& operator=(const EcheAppManager&) = delete;

  void BindSignalingMessageExchangerInterface(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  void BindUidGeneratorInterface(
      mojo::PendingReceiver<mojom::UidGenerator> receiver);

  void BindSystemInfoProviderInterface(
      mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

  void BindNotificationGeneratorInterface(
      mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

  AppsAccessManager* GetAppsAccessManager();

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<secure_channel::ConnectionManager> connection_manager_;
  std::unique_ptr<EcheFeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  std::unique_ptr<EcheNotificationClickHandler>
      eche_notification_click_handler_;
  std::unique_ptr<EcheConnector> eche_connector_;
  std::unique_ptr<EcheSignaler> signaler_;
  std::unique_ptr<EcheMessageReceiver> message_receiver_;
  std::unique_ptr<EchePresenceManager> eche_presence_manager_;
  std::unique_ptr<EcheUidProvider> uid_;
  std::unique_ptr<EcheRecentAppClickHandler> eche_recent_app_click_handler_;
  std::unique_ptr<EcheNotificationGenerator> notification_generator_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::unique_ptr<SystemInfoProvider> system_info_provider_;
  std::unique_ptr<AppsAccessManager> apps_access_manager_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_APP_MANAGER_H_
