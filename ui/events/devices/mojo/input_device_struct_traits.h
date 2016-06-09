// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_MOJO_INPUT_DEVICE_STRUCT_TRAITS_H_
#define UI_EVENTS_DEVICES_MOJO_INPUT_DEVICE_STRUCT_TRAITS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/mojo/input_devices.mojom.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<ui::mojom::InputDevice, ui::InputDevice> {
  static int32_t id(const ui::InputDevice& device) { return device.id; }

  static ui::mojom::InputDeviceType type(const ui::InputDevice& device) {
    switch (device.type) {
      case ui::INPUT_DEVICE_INTERNAL:
        return ui::mojom::InputDeviceType::INPUT_DEVICE_INTERNAL;
      case ui::INPUT_DEVICE_EXTERNAL:
        return ui::mojom::InputDeviceType::INPUT_DEVICE_EXTERNAL;
      case ui::INPUT_DEVICE_UNKNOWN:
        return ui::mojom::InputDeviceType::INPUT_DEVICE_UNKNOWN;
    }
  }

  static const std::string& name(const ui::InputDevice& device) {
    return device.name;
  }

  static base::StringPiece sys_path(const ui::InputDevice& device) {
    return device.sys_path.AsUTF8Unsafe();
  }

  static uint32_t vendor_id(const ui::InputDevice& device) {
    return device.vendor_id;
  }

  static uint32_t product_id(const ui::InputDevice& device) {
    return device.product_id;
  }

  static bool Read(ui::mojom::InputDeviceDataView data, ui::InputDevice* out) {
    out->id = data.id();

    switch (data.type()) {
      case ui::mojom::InputDeviceType::INPUT_DEVICE_INTERNAL:
        out->type = ui::INPUT_DEVICE_INTERNAL;
        break;
      case ui::mojom::InputDeviceType::INPUT_DEVICE_EXTERNAL:
        out->type = ui::INPUT_DEVICE_EXTERNAL;
        break;
      case ui::mojom::InputDeviceType::INPUT_DEVICE_UNKNOWN:
        out->type = ui::INPUT_DEVICE_UNKNOWN;
        break;
      default:
        // Who knows what values might come over the wire, fail if invalid.
        return false;
    }

    if (!data.ReadName(&out->name))
      return false;

    base::StringPiece sys_path_string;
    if (!data.ReadSysPath(&sys_path_string))
      return false;
    out->sys_path = base::FilePath::FromUTF8Unsafe(sys_path_string);

    out->vendor_id = data.vendor_id();
    out->product_id = data.product_id();

    return true;
  }
};

template <>
struct StructTraits<ui::mojom::TouchscreenDevice, ui::TouchscreenDevice> {
  static const ui::InputDevice& input_device(
      const ui::TouchscreenDevice& device) {
    return static_cast<const ui::InputDevice&>(device);
  }

  static const gfx::Size& size(const ui::TouchscreenDevice& device) {
    return device.size;
  }

  static int32_t touch_points(const ui::TouchscreenDevice& device) {
    return device.touch_points;
  }

  static bool Read(ui::mojom::TouchscreenDeviceDataView data,
                   ui::TouchscreenDevice* out) {
    if (!data.ReadInputDevice(static_cast<ui::InputDevice*>(out)))
      return false;

    if (!data.ReadSize(&out->size))
      return false;

    out->touch_points = data.touch_points();

    return true;
  }
};

}  // namespace mojo

#endif  // UI_EVENTS_DEVICES_MOJO_INPUT_DEVICE_STRUCT_TRAITS_H_
