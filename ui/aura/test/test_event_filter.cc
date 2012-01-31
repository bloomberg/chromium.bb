// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_event_filter.h"

namespace aura {
namespace test {

TestEventFilter::TestEventFilter()
    : key_event_count_(0),
      mouse_event_count_(0),
      touch_event_count_(0),
      consumes_key_events_(false),
      consumes_mouse_events_(false),
      consumes_touch_events_(false) {
}

TestEventFilter::~TestEventFilter() {
}

void TestEventFilter::ResetCounts() {
  key_event_count_ = 0;
  mouse_event_count_ = 0;
  touch_event_count_ = 0;
}

bool TestEventFilter::PreHandleKeyEvent(Window* target, KeyEvent* event) {
  ++key_event_count_;
  return consumes_key_events_;
}

bool TestEventFilter::PreHandleMouseEvent(Window* target, MouseEvent* event) {
  ++mouse_event_count_;
  return consumes_mouse_events_;
}

ui::TouchStatus TestEventFilter::PreHandleTouchEvent(Window* target,
                                                     TouchEvent* event) {
  ++touch_event_count_;
  // TODO(sadrul): !
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus TestEventFilter::PreHandleGestureEvent(Window* target,
                                                         GestureEvent* event) {
  // TODO(sad):
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace test
}  // namespace aura
