// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/gesture_event_android.h"

#include "ui/gfx/geometry/point_f.h"

namespace ui {

GestureEventAndroid::GestureEventAndroid(int type,
                                         const gfx::PointF& location,
                                         const gfx::PointF& screen_location,
                                         long time_ms,
                                         float delta)
    : type_(type),
      location_(location),
      screen_location_(screen_location),
      time_ms_(time_ms),
      delta_(delta) {}

GestureEventAndroid::~GestureEventAndroid() {}

std::unique_ptr<GestureEventAndroid> GestureEventAndroid::CreateFor(
    const gfx::PointF& new_location) const {
  auto offset = new_location - location_;
  gfx::PointF new_screen_location = screen_location_ + offset;
  return std::unique_ptr<GestureEventAndroid>(new GestureEventAndroid(
      type_, new_location, new_screen_location, time_ms_, delta_));
}

}  // namespace ui
