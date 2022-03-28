// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_
#define ASH_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_

#include <string>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash::secure_channel {

// Represents an attribute in the peripheral (service or characteristic).
struct RemoteAttribute {
  device::BluetoothUUID uuid;
  std::string id;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_
