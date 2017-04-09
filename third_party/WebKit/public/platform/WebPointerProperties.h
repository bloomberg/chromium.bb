// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPointerProperties_h
#define WebPointerProperties_h

#include <cstdint>
#include <limits>

namespace blink {

// This class encapsulates the properties that are common between mouse and
// pointer events and touch points as we transition towards the unified pointer
// event model.
// TODO(e_hakkinen): Replace WebTouchEvent with WebPointerEvent, remove
// WebTouchEvent and WebTouchPoint and merge this into WebPointerEvent.
class WebPointerProperties {
 public:
  enum class Button {
    kNoButton = -1,
    kLeft,
    kMiddle,
    kRight,
    kBack,
    kForward,
    kEraser
  };

  enum class Buttons : unsigned {
    kNoButton = 0,
    kLeft = 1 << 0,
    kRight = 1 << 1,
    kMiddle = 1 << 2,
    kBack = 1 << 3,
    kForward = 1 << 4,
    kEraser = 1 << 5
  };

  enum class PointerType {
    kUnknown,
    kMouse,
    kPen,
    kEraser,
    kTouch,
    kLastEntry = kTouch  // Must be the last entry in the list
  };

  WebPointerProperties()
      : id(0),
        force(std::numeric_limits<float>::quiet_NaN()),
        tilt_x(0),
        tilt_y(0),
        tangential_pressure(0.0f),
        twist(0),
        button(Button::kNoButton),
        pointer_type(PointerType::kUnknown),
        movement_x(0),
        movement_y(0) {}

  WebPointerProperties(Button button_param, PointerType pointer_type_param)
      : id(0),
        force(std::numeric_limits<float>::quiet_NaN()),
        tilt_x(0),
        tilt_y(0),
        tangential_pressure(0.0f),
        twist(0),
        button(button_param),
        pointer_type(pointer_type_param),
        movement_x(0),
        movement_y(0) {}

  int id;

  // The valid range is [0,1], with NaN meaning pressure is not supported by
  // the input device.
  float force;

  // Tilt of a pen stylus from surface normal as plane angles in degrees,
  // Values lie in [-90,90]. A positive tiltX is to the right and a positive
  // tiltY is towards the user.
  int tilt_x;
  int tilt_y;

  // The normalized tangential pressure (or barrel pressure), typically set by
  // an additional control of the stylus, which has a range of [-1,1], where 0
  // is the neutral position of the control. Always 0 if the device does not
  // support it.
  float tangential_pressure;

  // The clockwise rotation of a pen stylus around its own major axis, in
  // degrees in the range [0,359]. Always 0 if the device does not support it.
  int twist;

  // - For pointerup/down events, the button of pointing device that triggered
  // the event.
  // - For other events, the button that was depressed during the move event. If
  // multiple buttons were depressed, one of the depressed buttons (platform
  // dependent).
  Button button;

  PointerType pointer_type;

  int movement_x;
  int movement_y;
};

}  // namespace blink

#endif
