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
      key_event_handling_result_(ui::ER_UNHANDLED),
      mouse_event_handling_result_(ui::ER_UNHANDLED),
      consumes_touch_event_(false) {
}

TestEventFilter::~TestEventFilter() {
}

void TestEventFilter::ResetCounts() {
  key_event_count_ = 0;
  mouse_event_count_ = 0;
  touch_event_count_ = 0;
}

ui::EventResult TestEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  ++key_event_count_;
  return key_event_handling_result_;
}

ui::EventResult TestEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  ++mouse_event_count_;
  return mouse_event_handling_result_;
}

ui::EventResult TestEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::TouchStatus TestEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  ++touch_event_count_;
  // TODO(sadrul): !
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult TestEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(sadrul): !
  return ui::ER_UNHANDLED;
}

}  // namespace test
}  // namespace aura
