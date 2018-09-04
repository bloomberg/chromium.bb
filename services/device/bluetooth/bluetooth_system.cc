// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <memory>
#include <utility>

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

}  // namespace device
