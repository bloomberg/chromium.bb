// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_TOUCH_DEVICE_TRANSFORM_H_
#define UI_EVENTS_DEVICES_TOUCH_DEVICE_TRANSFORM_H_

#include <stdint.h>

#include "ui/events/devices/events_devices_export.h"
#include "ui/gfx/transform.h"

namespace ui {

struct EVENTS_DEVICES_EXPORT TouchDeviceTransform {
  TouchDeviceTransform();
  ~TouchDeviceTransform();

  int64_t display_id;
  int32_t device_id;
  gfx::Transform transform;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_TOUCH_DEVICE_TRANSFORM_H_
