// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_MOJO_TOUCH_DEVICE_TRANSFORM_STRUCT_TRAITS_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_MOJO_TOUCH_DEVICE_TRANSFORM_STRUCT_TRAITS_H_

#include <stdint.h>

#include "ui/display/manager/chromeos/mojo/touch_device_transform.mojom.h"
#include "ui/display/manager/chromeos/touch_device_transform.h"
#include "ui/gfx/mojo/transform_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<display::mojom::TouchDeviceTransformDataView,
                    display::TouchDeviceTransform> {
 public:
  static int64_t display_id(const display::TouchDeviceTransform& r) {
    return r.display_id;
  }
  static int32_t device_id(const display::TouchDeviceTransform& r) {
    return r.device_id;
  }
  static const gfx::Transform& transform(
      const display::TouchDeviceTransform& r) {
    return r.transform;
  }

  static bool Read(display::mojom::TouchDeviceTransformDataView data,
                   display::TouchDeviceTransform* out) {
    out->display_id = data.display_id();
    out->device_id = data.device_id();
    if (!data.ReadTransform(&(out->transform)))
      return false;
    return true;
  }
};

}  // namespace mojo

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_MOJO_TOUCH_DEVICE_TRANSFORM_STRUCT_TRAITS_H_
