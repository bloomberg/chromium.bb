// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMouseEvent_h
#define WebMouseEvent_h

#include "WebInputEvent.h"
#include "public/platform/WebMenuSourceType.h"

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
  static constexpr PointerId kMousePointerId = std::numeric_limits<int>::max();

  int click_count;

  // Only used for contextmenu events.
  WebMenuSourceType menu_source_type;

  WebMouseEvent(Type type_param,
                int x_param,
                int y_param,
                int global_x_param,
                int global_y_param,
                int modifiers_param,
                double time_stamp_seconds_param,
                PointerId id_param = kMousePointerId)
      : WebInputEvent(sizeof(WebMouseEvent),
                      type_param,
                      modifiers_param,
                      time_stamp_seconds_param),
        WebPointerProperties(id_param,
                             PointerType::kUnknown,
                             Button::kNoButton,
                             WebFloatPoint(x_param, y_param),
                             WebFloatPoint(global_x_param, global_y_param)) {
    DCHECK_GE(type_param, kMouseTypeFirst);
    DCHECK_LE(type_param, kMouseTypeLast);
  }

  WebMouseEvent(Type type_param,
                WebFloatPoint position,
                WebFloatPoint global_position,
                Button button_param,
                int click_count_param,
                int modifiers_param,
                double time_stamp_seconds_param,
                WebMenuSourceType menu_source_type_param = kMenuSourceNone,
                PointerId id_param = kMousePointerId)
      : WebInputEvent(sizeof(WebMouseEvent),
                      type_param,
                      modifiers_param,
                      time_stamp_seconds_param),
        WebPointerProperties(
            id_param,
            PointerType::kMouse,
            button_param,
            WebFloatPoint(floor(position.x), floor(position.y)),
            WebFloatPoint(floor(global_position.x), floor(global_position.y))),
        click_count(click_count_param),
        menu_source_type(menu_source_type_param) {
    DCHECK_GE(type_param, kMouseTypeFirst);
    DCHECK_LE(type_param, kMouseTypeLast);
  }

  WebMouseEvent(Type type_param,
                int modifiers_param,
                double time_stamp_seconds_param,
                PointerId id_param = kMousePointerId)
      : WebMouseEvent(sizeof(WebMouseEvent),
                      type_param,
                      modifiers_param,
                      time_stamp_seconds_param,
                      id_param) {}

  WebMouseEvent() : WebMouseEvent(sizeof(WebMouseEvent), kMousePointerId) {}

  bool FromTouch() const {
    return (GetModifiers() & kIsCompatibilityEventForTouch) != 0;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebMouseEvent(Type type_param,
                                      const WebGestureEvent&,
                                      Button button_param,
                                      int click_count_param,
                                      int modifiers_param,
                                      double time_stamp_seconds_param,
                                      PointerId id_param = kMousePointerId);

  BLINK_PLATFORM_EXPORT WebFloatPoint MovementInRootFrame() const;
  BLINK_PLATFORM_EXPORT WebFloatPoint PositionInRootFrame() const;

  // Sets any scaled values to be their computed values and sets |frame_scale_|
  // back to 1 and |frame_translate_| X and Y coordinates back to 0.
  BLINK_PLATFORM_EXPORT WebMouseEvent FlattenTransform() const;
#endif

  void SetPositionInWidget(float x, float y) {
    position_in_widget_ = WebFloatPoint(floor(x), floor(y));
  }

  void SetPositionInScreen(float x, float y) {
    position_in_screen_ = WebFloatPoint(floor(x), floor(y));
  }

 protected:
  WebMouseEvent(unsigned size_param, PointerId id_param)
      : WebInputEvent(size_param), WebPointerProperties(id_param) {}

  WebMouseEvent(unsigned size_param,
                Type type,
                int modifiers,
                double time_stamp_seconds,
                PointerId id_param)
      : WebInputEvent(size_param, type, modifiers, time_stamp_seconds),
        WebPointerProperties(id_param) {}

  void FlattenTransformSelf();

 private:
  void SetMenuSourceType(WebInputEvent::Type);
};

#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseEvent_h
