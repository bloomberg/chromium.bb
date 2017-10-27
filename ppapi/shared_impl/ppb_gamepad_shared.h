// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_

#include <stddef.h>

#include "base/atomicops.h"
#include "base/strings/string16.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace device {
class Gamepads;
}

namespace ppapi {

// TODO(brettw) when we remove the non-IPC-based gamepad implementation, this
// code should all move into the GamepadResource.

#pragma pack(push, 4)

struct WebKitGamepadButton {
  bool pressed;
  bool touched;
  double value;
};

enum WebKitGamepadHapticActuatorType {
  WEBKIT_GAMEPAD_HAPTIC_ACTUATOR_TYPE_VIBRATION = 0,
  WEBKIT_GAMEPAD_HAPTIC_ACTUATOR_TYPE_DUAL_RUMBLE = 1
};

struct WebKitGamepadHapticActuator {
  bool notNull;
  WebKitGamepadHapticActuatorType type;
};

struct WebKitGamepadVector {
  bool notNull;
  float x, y, z;
};

struct WebKitGamepadQuaternion {
  bool notNull;
  float x, y, z, w;
};

struct WebKitGamepadPose {
  bool notNull;

  bool hasOrientation;
  bool hasPosition;

  WebKitGamepadQuaternion orientation;
  WebKitGamepadVector position;
  WebKitGamepadVector angularVelocity;
  WebKitGamepadVector linearVelocity;
  WebKitGamepadVector angularAcceleration;
  WebKitGamepadVector linearAcceleration;
};

enum WebKitGamepadHand {
  WEBKIT_GAMEPAD_HAND_NONE = 0,
  WEBKIT_GAMEPAD_HAND_LEFT = 1,
  WEBKIT_GAMEPAD_HAND_RIGHT = 2
};

// This must match the definition of blink::Gamepad. The GamepadHost unit test
// has some compile asserts to validate this. Some members are unused but must
// be present to ensure the struct layout is the same.
struct WebKitGamepad {
  static const size_t kIdLengthCap = 128;
  static const size_t kMappingLengthCap = 16;
  static const size_t kAxesLengthCap = 16;
  static const size_t kButtonsLengthCap = 32;

  // Is there a gamepad connected at this index?
  bool connected;

  // Device identifier (based on manufacturer, model, etc.).
  base::char16 id[kIdLengthCap];

  // Monotonically increasing value referring to when the data were last
  // updated.
  unsigned long long timestamp;

  // Number of valid entries in the axes array.
  unsigned axes_length;

  // Normalized values representing axes, in the range [-1..1].
  double axes[kAxesLengthCap];

  // Number of valid entries in the buttons array.
  unsigned buttons_length;

  // Normalized values representing buttons, in the range [0..1].
  WebKitGamepadButton buttons[kButtonsLengthCap];

  // Gamepad Extensions member, unused by ppapi.
  WebKitGamepadHapticActuator vibrationActuator;

  // Mapping type (for example "standard")
  base::char16 mapping[kMappingLengthCap];

  // Gamepad Extensions member, unused by ppapi.
  WebKitGamepadPose pose;

  // Gamepad Extensions member, unused by ppapi.
  WebKitGamepadHand hand;

  // ID of the VRDisplay this gamepad is associated with, if any.
  unsigned display_id;
};

// This must match the definition of blink::Gamepads. The GamepadHost unit
// test has some compile asserts to validate this.
struct WebKitGamepads {
  static const size_t kItemsLengthCap = 4;

  // Gamepad data for N separate gamepad devices.
  WebKitGamepad items[kItemsLengthCap];
};

// This is the structure store in shared memory. It must match
// device::GamepadHardwareBuffer. The GamepadHost unit test has
// some compile asserts to validate this.
struct ContentGamepadHardwareBuffer {
  base::subtle::Atomic32 sequence;
  WebKitGamepads buffer;
};

#pragma pack(pop)

PPAPI_SHARED_EXPORT void ConvertWebKitGamepadData(
    const WebKitGamepads& webkit_data,
    PP_GamepadsSampleData* output_data);

PPAPI_SHARED_EXPORT void ConvertDeviceGamepadData(
    const device::Gamepads& device_data,
    PP_GamepadsSampleData* output_data);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_
