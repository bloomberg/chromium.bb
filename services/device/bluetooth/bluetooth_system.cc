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
  GetBluetoothAdapterClient()->AddObserver(this);

  std::vector<dbus::ObjectPath> object_paths =
      GetBluetoothAdapterClient()->GetAdapters();
  if (object_paths.empty())
    return;

  active_adapter_ = object_paths[0];
  auto* properties =
      GetBluetoothAdapterClient()->GetProperties(active_adapter_.value());
  state_ = properties->powered.value() ? State::kPoweredOn : State::kPoweredOff;
}

BluetoothSystem::~BluetoothSystem() = default;

void BluetoothSystem::AdapterAdded(const dbus::ObjectPath& object_path) {
  if (active_adapter_)
    return;

  active_adapter_ = object_path;
  UpdateStateAndNotifyIfNecessary();
}

void BluetoothSystem::AdapterRemoved(const dbus::ObjectPath& object_path) {
  DCHECK(active_adapter_);

  if (active_adapter_.value() != object_path)
    return;

  active_adapter_ = base::nullopt;

  std::vector<dbus::ObjectPath> object_paths =
      GetBluetoothAdapterClient()->GetAdapters();
  for (const auto& new_object_path : object_paths) {
    // The removed adapter is still included in GetAdapters().
    if (new_object_path == object_path)
      continue;

    active_adapter_ = new_object_path;
    break;
  }

  UpdateStateAndNotifyIfNecessary();
}

void BluetoothSystem::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DCHECK(active_adapter_);
  if (active_adapter_.value() != object_path)
    return;

  auto* properties =
      GetBluetoothAdapterClient()->GetProperties(active_adapter_.value());

  if (properties->powered.name() == property_name)
    UpdateStateAndNotifyIfNecessary();
}

void BluetoothSystem::GetState(GetStateCallback callback) {
  std::move(callback).Run(state_);
}

bluez::BluetoothAdapterClient* BluetoothSystem::GetBluetoothAdapterClient() {
  // Use AlternateBluetoothAdapterClient to avoid interfering with users of the
  // regular BluetoothAdapterClient.
  return bluez::BluezDBusManager::Get()->GetAlternateBluetoothAdapterClient();
}

void BluetoothSystem::UpdateStateAndNotifyIfNecessary() {
  State old_state = state_;
  if (active_adapter_) {
    auto* properties =
        GetBluetoothAdapterClient()->GetProperties(active_adapter_.value());
    state_ =
        properties->powered.value() ? State::kPoweredOn : State::kPoweredOff;
  } else {
    state_ = State::kUnavailable;
  }

  if (old_state != state_)
    client_ptr_->OnStateChanged(state_);
}

}  // namespace device
