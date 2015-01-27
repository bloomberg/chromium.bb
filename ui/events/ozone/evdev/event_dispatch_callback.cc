// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_dispatch_callback.h"

namespace ui {

KeyEventParams::KeyEventParams(int device_id, unsigned int code, bool down)
    : device_id(device_id), code(code), down(down) {
}

KeyEventParams::KeyEventParams(const KeyEventParams& other) = default;

KeyEventParams::~KeyEventParams() {
}

TouchEventParams::TouchEventParams(int device_id,
                                   int touch_id,
                                   EventType type,
                                   const gfx::PointF& location,
                                   const gfx::Vector2dF& radii,
                                   float pressure,
                                   const base::TimeDelta& timestamp)
    : device_id(device_id),
      touch_id(touch_id),
      type(type),
      location(location),
      radii(radii),
      pressure(pressure),
      timestamp(timestamp) {
}

TouchEventParams::TouchEventParams(const TouchEventParams& other) = default;

TouchEventParams::~TouchEventParams() {
}

}  // namspace ui
