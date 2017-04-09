// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMouseEvent_h
#define WebMouseEvent_h

#include "WebInputEvent.h"

namespace blink {

class WebGestureEvent;

// See WebInputEvent.h for details why this pack is here.
#pragma pack(push, 4)

// WebMouseEvent --------------------------------------------------------------

// TODO(mustaq): We are truncating |float|s to integers whenever setting
//   coordinate values, to avoid regressions for now. Will be fixed later
//   on. crbug.com/456625
class WebMouseEvent : public WebInputEvent, public WebPointerProperties {
 public:
  int click_count;

  WebMouseEvent(Type type_param,
                int x_param,
                int y_param,
                int global_x_param,
                int global_y_param,
                int modifiers_param,
                double time_stamp_seconds_param)
      : WebInputEvent(sizeof(WebMouseEvent),
                      type_param,
                      modifiers_param,
                      time_stamp_seconds_param),
        WebPointerProperties(),
        position_in_widget_(x_param, y_param),
        position_in_screen_(global_x_param, global_y_param) {}

  WebMouseEvent(Type type_param,
                WebFloatPoint position,
                WebFloatPoint global_position,
                Button button_param,
                int click_count_param,
                int modifiers_param,
                double time_stamp_seconds_param)
      : WebInputEvent(sizeof(WebMouseEvent),
                      type_param,
                      modifiers_param,
                      time_stamp_seconds_param),
        WebPointerProperties(button_param, PointerType::kMouse),
        click_count(click_count_param),
        position_in_widget_(floor(position.x), floor(position.y)),
        position_in_screen_(floor(global_position.x),
                            floor(global_position.y)) {}

  WebMouseEvent(Type type_param,
                int modifiers_param,
                double time_stamp_seconds_param)
      : WebMouseEvent(sizeof(WebMouseEvent),
                      type_param,
                      modifiers_param,
                      time_stamp_seconds_param) {}

  WebMouseEvent() : WebMouseEvent(sizeof(WebMouseEvent)) {}

  bool FromTouch() const {
    return (GetModifiers() & kIsCompatibilityEventForTouch) != 0;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebMouseEvent(Type type_param,
                                      const WebGestureEvent&,
                                      Button button_param,
                                      int click_count_param,
                                      int modifiers_param,
                                      double time_stamp_seconds_param);

  BLINK_PLATFORM_EXPORT WebFloatPoint MovementInRootFrame() const;
  BLINK_PLATFORM_EXPORT WebFloatPoint PositionInRootFrame() const;

  // Sets any scaled values to be their computed values and sets |frameScale|
  // back to 1 and |translateX|, |translateY| back to 0.
  BLINK_PLATFORM_EXPORT WebMouseEvent FlattenTransform() const;
#endif

  WebFloatPoint PositionInWidget() const { return position_in_widget_; }
  void SetPositionInWidget(float x, float y) {
    position_in_widget_ = WebFloatPoint(floor(x), floor(y));
  }

  WebFloatPoint PositionInScreen() const { return position_in_screen_; }
  void SetPositionInScreen(float x, float y) {
    position_in_screen_ = WebFloatPoint(floor(x), floor(y));
  }

 protected:
  explicit WebMouseEvent(unsigned size_param)
      : WebInputEvent(size_param), WebPointerProperties() {}

  WebMouseEvent(unsigned size_param,
                Type type,
                int modifiers,
                double time_stamp_seconds)
      : WebInputEvent(size_param, type, modifiers, time_stamp_seconds),
        WebPointerProperties() {}

  void FlattenTransformSelf();

 private:
  // Widget coordinate, which is relative to the bound of current RenderWidget
  // (e.g. a plugin or OOPIF inside a RenderView). Similar to viewport
  // coordinates but without DevTools emulation transform or overscroll applied.
  WebFloatPoint position_in_widget_;

  // Screen coordinate
  WebFloatPoint position_in_screen_;
};

#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseEvent_h
