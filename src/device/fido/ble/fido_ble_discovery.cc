// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_device.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_device_authenticator.h"

namespace device {

FidoBleDiscovery::FidoBleDiscovery()
    : FidoBleDiscoveryBase(FidoTransportProtocol::kBluetoothLowEnergy),
      weak_factory_(this) {}

FidoBleDiscovery::~FidoBleDiscovery() = default;

// static
const BluetoothUUID& FidoBleDiscovery::FidoServiceUUID() {
  static const BluetoothUUID service_uuid(kFidoServiceUUID);
  return service_uuid;
}

void FidoBleDiscovery::OnSetPowered() {
  DCHECK(adapter());
  VLOG(2) << "Adapter " << adapter()->GetAddress() << " is powered on.";

  for (BluetoothDevice* device : adapter()->GetDevices()) {
    if (!CheckForExcludedDeviceAndCacheAddress(device) &&
        base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
      VLOG(2) << "U2F BLE device: " << device->GetAddress();
      AddDevice(
          std::make_unique<FidoBleDevice>(adapter(), device->GetAddress()));
    }
  }

  auto filter = std::make_unique<BluetoothDiscoveryFilter>(
      BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
  filter->AddUUID(FidoServiceUUID());

  adapter()->StartDiscoverySessionWithFilter(
      std::move(filter),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoBleDiscovery::OnStartDiscoverySessionWithFilter,
                         weak_factory_.GetWeakPtr())),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoBleDiscovery::OnStartDiscoverySessionError,
                         weak_factory_.GetWeakPtr())));
}

void FidoBleDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                   BluetoothDevice* device) {
  if (!CheckForExcludedDeviceAndCacheAddress(device) &&
      base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
    VLOG(2) << "Discovered U2F BLE device: " << device->GetAddress();
    AddDevice(std::make_unique<FidoBleDevice>(adapter, device->GetAddress()));
  }
}

void FidoBleDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (CheckForExcludedDeviceAndCacheAddress(device) ||
      !base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
    return;
  }

  const auto device_id = FidoBleDevice::GetId(device->GetAddress());
  auto* authenticator = GetAuthenticator(device_id);
  if (!authenticator) {
    VLOG(2) << "Discovered U2F service on existing BLE device: "
            << device->GetAddress();
    AddDevice(std::make_unique<FidoBleDevice>(adapter, device->GetAddress()));
    return;
  }

  // Our model of FIDO BLE security key assumes that if BLE device is in pairing
  // mode long enough time without pairing attempt, the device stops advertising
  // and BluetoothAdapter::DeviceRemoved() is invoked instead of returning back
  // to regular "non-pairing" mode. As so, we only notify observer when
  // |fido_device| goes into pairing mode.
  if (observer() && authenticator->device()->IsInPairingMode())
    observer()->AuthenticatorPairingModeChanged(this, device_id);
}

void FidoBleDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
    VLOG(2) << "U2F BLE device removed: " << device->GetAddress();
    RemoveDevice(FidoBleDevice::GetId(device->GetAddress()));
  }
}

void FidoBleDiscovery::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                             bool powered) {
  // If Bluetooth adapter is powered on, resume scanning for nearby FIDO
  // devices. Previously inactive discovery sessions would be terminated upon
  // invocation of OnSetPowered().
  if (powered)
    OnSetPowered();
}

void FidoBleDiscovery::DeviceAddressChanged(BluetoothAdapter* adapter,
                                            BluetoothDevice* device,
                                            const std::string& old_address) {
  auto previous_device_id = FidoBleDevice::GetId(old_address);
  auto new_device_id = FidoBleDevice::GetId(device->GetAddress());
  auto it = authenticators_.find(previous_device_id);
  if (it == authenticators_.end())
    return;

  VLOG(2) << "Discovered FIDO BLE device address change from old address : "
          << old_address << " to new address : " << device->GetAddress();
  authenticators_.emplace(new_device_id, std::move(it->second));
  authenticators_.erase(it);

  if (observer()) {
    observer()->AuthenticatorIdChanged(this, previous_device_id,
                                       std::move(new_device_id));
  }
}

bool FidoBleDiscovery::CheckForExcludedDeviceAndCacheAddress(
    const BluetoothDevice* device) {
  std::string device_address = device->GetAddress();
  auto address_position =
      excluded_cable_device_addresses_.lower_bound(device_address);
  if (address_position != excluded_cable_device_addresses_.end() &&
      *address_position == device_address) {
    return true;
  }

  // IsCableDevice() is not stable, and can change throughout the lifetime. As
  // so, cache device address for known Cable devices so that we do not attempt
  // to connect to these devices.
  if (IsCableDevice(device)) {
    excluded_cable_device_addresses_.insert(address_position,
                                            std::move(device_address));
    return true;
  }

  return false;
}

}  // namespace device
