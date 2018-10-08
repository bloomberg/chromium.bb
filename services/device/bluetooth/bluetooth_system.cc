// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <memory>
#include <utility>
#include <vector>

#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

void BluetoothSystem::Create(mojom::BluetoothSystemRequest request,
                             mojom::BluetoothSystemClientPtr client) {
  mojo::MakeStrongBinding(std::make_unique<BluetoothSystem>(std::move(client)),
                          std::move(request));
}

BluetoothSystem::BluetoothSystem(mojom::BluetoothSystemClientPtr client) {
  client_ptr_ = std::move(client);
}

BluetoothSystem::~BluetoothSystem() = default;

void BluetoothSystem::GetState(GetStateCallback callback) {
  std::vector<dbus::ObjectPath> object_paths =
      GetBluetoothAdapterClient()->GetAdapters();
  if (object_paths.empty()) {
    std::move(callback).Run(State::kUnavailable);
    return;
  }

  auto* properties =
      GetBluetoothAdapterClient()->GetProperties(object_paths[0]);
  std::move(callback).Run(properties->powered.value() ? State::kPoweredOn
                                                      : State::kPoweredOff);
}

bluez::BluetoothAdapterClient* BluetoothSystem::GetBluetoothAdapterClient() {
  // Use AlternateBluetoothAdapterClient to avoid interfering with users of the
  // regular BluetoothAdapterClient.
  return bluez::BluezDBusManager::Get()->GetAlternateBluetoothAdapterClient();
}

}  // namespace device
