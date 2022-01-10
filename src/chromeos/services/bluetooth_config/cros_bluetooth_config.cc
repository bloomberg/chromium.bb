// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"

#include "chromeos/services/bluetooth_config/bluetooth_device_status_notifier_impl.h"
#include "chromeos/services/bluetooth_config/bluetooth_power_controller.h"
#include "chromeos/services/bluetooth_config/device_name_manager.h"
#include "chromeos/services/bluetooth_config/device_operation_handler.h"
#include "chromeos/services/bluetooth_config/discovery_session_manager.h"
#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/services/bluetooth_config/initializer.h"
#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace bluetooth_config {

CrosBluetoothConfig::CrosBluetoothConfig(
    Initializer& initializer,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    FastPairDelegate* fast_pair_delegate)
    : adapter_state_controller_(
          initializer.CreateAdapterStateController(bluetooth_adapter)),
      bluetooth_power_controller_(initializer.CreateBluetoothPowerController(
          adapter_state_controller_.get())),
      device_name_manager_(
          initializer.CreateDeviceNameManager(bluetooth_adapter)),
      device_cache_(
          initializer.CreateDeviceCache(adapter_state_controller_.get(),
                                        bluetooth_adapter,
                                        device_name_manager_.get())),
      system_properties_provider_(
          std::make_unique<SystemPropertiesProviderImpl>(
              adapter_state_controller_.get(),
              device_cache_.get())),
      bluetooth_device_status_notifier_(
          initializer.CreateBluetoothDeviceStatusNotifier(device_cache_.get())),
      discovery_session_manager_(initializer.CreateDiscoverySessionManager(
          adapter_state_controller_.get(),
          bluetooth_adapter,
          device_cache_.get())),
      device_operation_handler_(initializer.CreateDeviceOperationHandler(
          adapter_state_controller_.get(),
          bluetooth_adapter)),
      fast_pair_delegate_(fast_pair_delegate) {
  if (fast_pair_delegate_)
    fast_pair_delegate_->SetDeviceNameManager(device_name_manager_.get());
}

CrosBluetoothConfig::~CrosBluetoothConfig() {
  if (fast_pair_delegate_)
    fast_pair_delegate_->SetDeviceNameManager(nullptr);
}

void CrosBluetoothConfig::SetPrefs(PrefService* logged_in_profile_prefs,
                                   PrefService* local_state) {
  bluetooth_power_controller_->SetPrefs(logged_in_profile_prefs, local_state);
  device_name_manager_->SetPrefs(local_state);
}

void CrosBluetoothConfig::BindPendingReceiver(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void CrosBluetoothConfig::ObserveSystemProperties(
    mojo::PendingRemote<mojom::SystemPropertiesObserver> observer) {
  system_properties_provider_->Observe(std::move(observer));
}

void CrosBluetoothConfig::ObserveDeviceStatusChanges(
    mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver> observer) {
  bluetooth_device_status_notifier_->ObserveDeviceStatusChanges(
      std::move(observer));
}

void CrosBluetoothConfig::SetBluetoothEnabledState(bool enabled) {
  bluetooth_power_controller_->SetBluetoothEnabledState(enabled);
}

void CrosBluetoothConfig::StartDiscovery(
    mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate) {
  discovery_session_manager_->StartDiscovery(std::move(delegate));
}

void CrosBluetoothConfig::Connect(
    const std::string& device_id,
    CrosBluetoothConfig::ConnectCallback callback) {
  device_operation_handler_->Connect(device_id, std::move(callback));
}

void CrosBluetoothConfig::Disconnect(
    const std::string& device_id,
    CrosBluetoothConfig::DisconnectCallback callback) {
  device_operation_handler_->Disconnect(device_id, std::move(callback));
}

void CrosBluetoothConfig::Forget(const std::string& device_id,
                                 CrosBluetoothConfig::ForgetCallback callback) {
  device_operation_handler_->Forget(device_id, std::move(callback));
}

void CrosBluetoothConfig::SetDeviceNickname(const std::string& device_id,
                                            const std::string& nickname) {
  device_name_manager_->SetDeviceNickname(device_id, nickname);
}

}  // namespace bluetooth_config
}  // namespace chromeos
