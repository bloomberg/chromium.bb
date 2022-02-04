// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/discovery_session_manager.h"

#include "base/bind.h"

namespace chromeos {
namespace bluetooth_config {

DiscoverySessionManager::DiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    DiscoveredDevicesProvider* discovered_devices_provider)
    : adapter_state_controller_(adapter_state_controller),
      discovered_devices_provider_(discovered_devices_provider) {
  adapter_state_controller_observation_.Observe(adapter_state_controller_);
  discovered_devices_provider_observation_.Observe(
      discovered_devices_provider_);
  delegates_.set_disconnect_handler(
      base::BindRepeating(&DiscoverySessionManager::OnDelegateDisconnected,
                          base::Unretained(this)));
}

DiscoverySessionManager::~DiscoverySessionManager() = default;

void DiscoverySessionManager::StartDiscovery(
    mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate) {
  // If Bluetooth is not enabled, we cannot start discovery.
  if (!IsBluetoothEnabled()) {
    mojo::Remote<mojom::BluetoothDiscoveryDelegate> delegate_remote(
        std::move(delegate));
    delegate_remote->OnBluetoothDiscoveryStopped();
    return;
  }

  const bool had_client_before_call = HasAtLeastOneDiscoveryClient();

  mojo::RemoteSetElementId id = delegates_.Add(std::move(delegate));

  // The number of clients has increased from 0 to 1.
  if (!had_client_before_call)
    OnHasAtLeastOneDiscoveryClientChanged();

  // If discovery is already active, notify the delegate that discovery has
  // started and of the current discovered devices list.
  if (IsDiscoverySessionActive()) {
    delegates_.Get(id)->OnBluetoothDiscoveryStarted(
        RegisterNewDevicePairingHandler(id));
    delegates_.Get(id)->OnDiscoveredDevicesListChanged(
        discovered_devices_provider_->GetDiscoveredDevices());
  }
}

void DiscoverySessionManager::NotifyDiscoveryStarted() {
  for (auto it = delegates_.begin(); it != delegates_.end(); ++it) {
    (*it)->OnBluetoothDiscoveryStarted(
        RegisterNewDevicePairingHandler(it.id()));
  }
}

void DiscoverySessionManager::NotifyDiscoveryStoppedAndClearActiveClients() {
  if (!HasAtLeastOneDiscoveryClient())
    return;

  for (auto& delegate : delegates_)
    delegate->OnBluetoothDiscoveryStopped();

  // Since discovery has stopped, disconnect all delegates and handlers since
  // they are no longer actionable.
  delegates_.Clear();
  id_to_pairing_handler_map_.clear();

  // The number of clients has decreased from >0 to 0.
  OnHasAtLeastOneDiscoveryClientChanged();
}

bool DiscoverySessionManager::HasAtLeastOneDiscoveryClient() const {
  return !delegates_.empty();
}

void DiscoverySessionManager::NotifyDiscoveredDevicesListChanged() {
  for (auto& delegate : delegates_) {
    delegate->OnDiscoveredDevicesListChanged(
        discovered_devices_provider_->GetDiscoveredDevices());
  }
}

void DiscoverySessionManager::OnAdapterStateChanged() {
  // We only need to handle the case where we have at least one client and
  // Bluetooth is no longer enabled.
  if (!HasAtLeastOneDiscoveryClient() || IsBluetoothEnabled())
    return;

  NotifyDiscoveryStoppedAndClearActiveClients();
}

void DiscoverySessionManager::OnDiscoveredDevicesListChanged() {
  NotifyDiscoveredDevicesListChanged();
}

mojo::PendingRemote<mojom::DevicePairingHandler>
DiscoverySessionManager::RegisterNewDevicePairingHandler(
    mojo::RemoteSetElementId id) {
  mojo::PendingRemote<mojom::DevicePairingHandler> remote;
  id_to_pairing_handler_map_[id] = CreateDevicePairingHandler(
      adapter_state_controller_, remote.InitWithNewPipeAndPassReceiver(),
      base::BindOnce(&DiscoverySessionManager::OnPairingFinished,
                     weak_ptr_factory_.GetWeakPtr(), id));
  return remote;
}

void DiscoverySessionManager::OnPairingFinished(mojo::RemoteSetElementId id) {
  // This can be called when we delete a handler such as if it is disconnected
  // or discovery stops. At this point, delegates_ won't contain |id| anymore.
  if (!delegates_.Contains(id))
    return;

  delegates_.Remove(id);

  // Manually call the disconnect handler since it's not automatically called.
  OnDelegateDisconnected(id);
}

bool DiscoverySessionManager::IsBluetoothEnabled() const {
  return adapter_state_controller_->GetAdapterState() ==
         mojom::BluetoothSystemState::kEnabled;
}

void DiscoverySessionManager::OnDelegateDisconnected(
    mojo::RemoteSetElementId id) {
  id_to_pairing_handler_map_.erase(id);

  // If the disconnected client was the last one, the number of clients has
  // decreased from 1 to 0.
  if (!HasAtLeastOneDiscoveryClient())
    OnHasAtLeastOneDiscoveryClientChanged();
}

void DiscoverySessionManager::FlushForTesting() {
  delegates_.FlushForTesting();  // IN-TEST
}

}  // namespace bluetooth_config
}  // namespace chromeos
