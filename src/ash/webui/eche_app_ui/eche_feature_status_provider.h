// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_

#include "ash/components/phonehub/feature_status_provider.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/memory/weak_ptr.h"

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace eche_app {

// FeatureStatusProvider implementation which observes PhoneHub's state, then
// layers in Eche's state.
class EcheFeatureStatusProvider
    : public FeatureStatusProvider,
      public phonehub::FeatureStatusProvider::Observer,
      public secure_channel::ConnectionManager::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  EcheFeatureStatusProvider(
      phonehub::PhoneHubManager* phone_hub_manager,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager);
  ~EcheFeatureStatusProvider() override;

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

 private:
  void UpdateStatus();
  FeatureStatus ComputeStatus();

  // phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  phonehub::FeatureStatusProvider* phone_hub_feature_status_provider_;
  device_sync::DeviceSyncClient* device_sync_client_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  secure_channel::ConnectionManager* connection_manager_;
  phonehub::FeatureStatus current_phone_hub_feature_status_;
  absl::optional<FeatureStatus> status_;
  base::WeakPtrFactory<EcheFeatureStatusProvider> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_
