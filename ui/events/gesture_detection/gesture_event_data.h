// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_

#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"

namespace ui {

class GestureEventDataPacket;

// Simple transport construct for gesture-related event data.
// TODO(jdduke): Merge this class with ui::GestureEventDetails.
struct GESTURE_DETECTION_EXPORT GestureEventData {
  struct Details;
  GestureEventData(EventType type,
                   base::TimeTicks time,
                   float x,
                   float y,
                   const Details& details);

  EventType type;
  base::TimeTicks time;
  float x;
  float y;

  // TODO(jdduke): Determine if we can simply re-use blink::WebGestureEvent, as
  // this is more or less straight up duplication.
  struct GESTURE_DETECTION_EXPORT Details {
    Details();
    union {
      struct {
        int tap_count;
        float width;
        float height;
      } tap;

      struct {
        float width;
        float height;
      } tap_down;

      struct {
        float width;
        float height;
      } show_press;

      struct {
        float width;
        float height;
      } long_press;

      struct {
        // Initial motion that triggered the scroll.
        // May be redundant with delta_x/delta_y in the first scroll_update.
        float delta_x_hint;
        float delta_y_hint;
      } scroll_begin;

      struct {
        float delta_x;
        float delta_y;
        float velocity_x;
        float velocity_y;
      } scroll_update;

      struct {
        float velocity_x;
        float velocity_y;
      } fling_start;

      struct {
        float scale;
      } pinch_update;
    };
  } details;

 private:
  friend class GestureEventDataPacket;

  // Initializes type to GESTURE_TYPE_INVALID.
  GestureEventData();
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
