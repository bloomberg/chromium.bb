// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_PARAMS_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_PARAMS_H_

#include "base/time/time.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"

namespace ui {

enum GestureEventType {
  GESTURE_SHOW_PRESS,
  GESTURE_DOUBLE_TAP,
  GESTURE_SINGLE_TAP_CONFIRMED,
  GESTURE_SINGLE_TAP_UNCONFIRMED,
  GESTURE_LONG_PRESS,
  GESTURE_SCROLL_BEGIN,
  GESTURE_SCROLL_UPDATE,
  GESTURE_SCROLL_END,
  GESTURE_FLING_START,
  GESTURE_FLING_CANCEL,
  GESTURE_PINCH_BEGIN,
  GESTURE_PINCH_UPDATE,
  GESTURE_PINCH_END,
  GESTURE_TAP_CANCEL,
  GESTURE_LONG_TAP,
  GESTURE_TAP_DOWN
};

// TODO(jdduke): Convert all (x,y) and (width,height) pairs to their
// corresponding gfx:: geometry types.
struct GESTURE_DETECTION_EXPORT GestureEventParams {
  struct Data;
  GestureEventParams(GestureEventType type,
                     base::TimeTicks time,
                     float x,
                     float y,
                     const Data& data);

  GestureEventType type;
  base::TimeTicks time;
  float x;
  float y;

  // TODO(jdduke): Determine if we can simply re-use blink::WebGestureEvent, as
  // this is more or less straight up duplication.
  struct Data {
    Data();
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
        // May be redundant with deltaX/deltaY in the first scrollUpdate.
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
  } data;

 private:
  GestureEventParams();
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_PARAMS_H_
