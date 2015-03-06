// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TEST_EVENT_MATCHERS_H_
#define REMOTING_PROTOCOL_TEST_EVENT_MATCHERS_H_

#include "testing/gmock/include/gmock/gmock.h"

// This file contains matchers for events.
namespace remoting {
namespace protocol {

MATCHER_P(TouchEventEqual, expected_event, "Expect touch events equal.") {
  if (arg.event_type() != expected_event.event_type())
    return false;

  if (arg.touch_points().size() != expected_event.touch_points().size())
    return false;

  for (int i = 0; i < expected_event.touch_points().size(); ++i) {
    const protocol::TouchEventPoint& expected_point =
        expected_event.touch_points(i);
    const protocol::TouchEventPoint& actual_point = arg.touch_points(i);

    const bool equal = expected_point.id() == actual_point.id() &&
                       expected_point.x() == actual_point.x() &&
                       expected_point.y() == actual_point.y() &&
                       expected_point.radius_x() == actual_point.radius_x() &&
                       expected_point.radius_y() == actual_point.radius_y() &&
                       expected_point.angle() == actual_point.angle() &&
                       expected_point.pressure() == actual_point.pressure();
    if (!equal)
      return false;
  }

  return true;
}

MATCHER(IsTouchCancelEvent, "expect touch cancel event") {
  return arg.event_type() == protocol::TouchEvent::TOUCH_POINT_CANCEL;
}

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TEST_EVENT_MATCHERS_H_
