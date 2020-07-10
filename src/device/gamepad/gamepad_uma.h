// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_UMA_H_
#define DEVICE_GAMEPAD_GAMEPAD_UMA_H_

#include <stddef.h>
#include <stdint.h>

#include "device/gamepad/gamepad_pad_state_provider.h"

namespace device {

// Compare the |vendor_id| and |product_id| of a connected USB or Bluetooth
// device against a list of known gaming peripherals. If a match is found,
// record a GamepadId enumeration value corresponding to the device. Does
// nothing if the device has no corresponding GamepadId.
//
// To preserve privacy, the vendor and product IDs are not recorded.
void RecordConnectedGamepad(uint16_t vendor_id, uint16_t product_id);

// Record that the gamepad data fetcher identified by |source| recognized a
// device as a gamepad, but the device is not included on our list of known
// gamepads.
void RecordUnknownGamepad(GamepadSource source);

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_UMA_H_
