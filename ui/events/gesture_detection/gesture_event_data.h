// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_

#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_event_details.h"

namespace ui {

class GestureEventDataPacket;

struct GESTURE_DETECTION_EXPORT GestureEventData {
  GestureEventData(EventType type,
                   int motion_event_id,
                   base::TimeTicks time,
                   float x,
                   float y,
                   int touch_point_count,
                   const GestureEventDetails& details);

  GestureEventData(EventType type,
                   int motion_event_id,
                   base::TimeTicks time,
                   float x,
                   float y,
                   int touch_point_count);

  EventType type;
  int motion_event_id;
  base::TimeTicks time;
  float x;
  float y;

  GestureEventDetails details;

 private:
  friend class GestureEventDataPacket;

  // Initializes type to GESTURE_TYPE_INVALID.
  GestureEventData();
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
