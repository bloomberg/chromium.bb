// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_H_
#define SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_H_

#include "base/macros.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace device {

class BluetoothSystem : public mojom::BluetoothSystem {
 public:
  static void Create(mojom::BluetoothSystemRequest request,
                     mojom::BluetoothSystemClientPtr client);

  explicit BluetoothSystem(mojom::BluetoothSystemClientPtr client);
  ~BluetoothSystem() override;

 private:
  mojom::BluetoothSystemClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSystem);
};

}  // namespace device

#endif  // SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_H_
