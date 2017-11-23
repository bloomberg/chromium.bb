// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_GESTURE_EVENT_ANDROID_H_
#define UI_EVENTS_ANDROID_GESTURE_EVENT_ANDROID_H_

#include "base/macros.h"
#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// Event class used to carry the info that match the blink::WebGestureEvent.
// This was devised because we can't use the blink type in ViewAndroid tree
// since hit testing requires templated things to modify these events.
// All coordinates are in DIPs.
class EVENTS_EXPORT GestureEventAndroid {
 public:
  GestureEventAndroid(int type,
                      const gfx::PointF& location,
                      const gfx::PointF& screen_location,
                      long time_ms,
                      float delta);

  ~GestureEventAndroid();

  int type() const { return type_; }
  const gfx::PointF& location() const { return location_; }
  const gfx::PointF& screen_location() const { return screen_location_; }
  long time() const { return time_ms_; }
  float delta() const { return delta_; }

  // Creates a new GestureEventAndroid instance different from |this| only by
  // its location.
  std::unique_ptr<GestureEventAndroid> CreateFor(
      const gfx::PointF& new_location) const;

 private:
  int type_;
  gfx::PointF location_;
  gfx::PointF screen_location_;
  long time_ms_;

  float delta_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventAndroid);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_GESTURE_EVENT_ANDROID_H_
