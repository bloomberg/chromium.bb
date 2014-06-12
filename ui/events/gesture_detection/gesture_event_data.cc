// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data.h"

#include "base/logging.h"

namespace ui {

GestureEventData::GestureEventData(const GestureEventDetails& details,
                                   int motion_event_id,
                                   base::TimeTicks time,
                                   float x,
                                   float y,
                                   int touch_point_count,
                                   const gfx::RectF& bounding_box)
    : details(details),
      motion_event_id(motion_event_id),
      time(time),
      x(x),
      y(y) {
  DCHECK_GE(motion_event_id, 0);
  DCHECK_NE(0, touch_point_count);
  this->details.set_touch_points(touch_point_count);
  this->details.set_bounding_box(bounding_box);
}

GestureEventData::GestureEventData(EventType type,
                                   int motion_event_id,
                                   base::TimeTicks time,
                                   float x,
                                   float y,
                                   int touch_point_count,
                                   const gfx::RectF& bounding_box)
    : details(GestureEventDetails(type, 0, 0)),
      motion_event_id(motion_event_id),
      time(time),
      x(x),
      y(y) {
  DCHECK_GE(motion_event_id, 0);
  details.set_touch_points(touch_point_count);
  details.set_bounding_box(bounding_box);
}

GestureEventData::GestureEventData() : x(0), y(0) {
}

}  //  namespace ui
