// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_

#include "base/time/time.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"

namespace ui {

// TODO(jdduke): Consider adoption of ui::EventType.
enum GestureEventType {
  GESTURE_TYPE_INVALID = -1,
  GESTURE_TYPE_FIRST = 0,
  GESTURE_TAP_DOWN = GESTURE_TYPE_FIRST,
  GESTURE_TAP_UNCONFIRMED,
  GESTURE_TAP,
  GESTURE_DOUBLE_TAP,
  GESTURE_TAP_CANCEL,
  GESTURE_SHOW_PRESS,
  GESTURE_LONG_TAP,
  GESTURE_LONG_PRESS,
  GESTURE_SCROLL_BEGIN,
  GESTURE_SCROLL_UPDATE,
  GESTURE_SCROLL_END,
  GESTURE_FLING_START,
  GESTURE_FLING_CANCEL,
  GESTURE_PINCH_BEGIN,
  GESTURE_PINCH_UPDATE,
  GESTURE_PINCH_END,
  GESTURE_TYPE_LAST = GESTURE_PINCH_END,
};

class GestureEventDataPacket;

// Simple transport construct for gesture-related event data.
// TODO(jdduke): Merge this class with ui::GestureEventDetails.
struct GESTURE_DETECTION_EXPORT GestureEventData {
  struct Details;
  GestureEventData(GestureEventType type,
                   base::TimeTicks time,
                   float x,
                   float y,
                   const Details& details);

  GestureEventType type;
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
