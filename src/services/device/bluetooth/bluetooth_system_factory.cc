// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system_factory.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/bluetooth/bluetooth_system.h"

namespace device {

void BluetoothSystemFactory::CreateFactory(
    mojom::BluetoothSystemFactoryRequest request) {
  mojo::MakeStrongBinding(std::make_unique<BluetoothSystemFactory>(),
                          std::move(request));
}

BluetoothSystemFactory::BluetoothSystemFactory() = default;

BluetoothSystemFactory::~BluetoothSystemFactory() = default;

void BluetoothSystemFactory::Create(
    mojom::BluetoothSystemRequest system_request,
    mojom::BluetoothSystemClientPtr system_client) {
  BluetoothSystem::Create(std::move(system_request), std::move(system_client));
}

}  // namespace device
