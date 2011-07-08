// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_scrollbar.h"

#include <cstring>

#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Scrollbar);

TestScrollbar::TestScrollbar(TestingInstance* instance)
    : TestCase(instance),
      WidgetClient_Dev(instance),
      scrollbar_(*instance, true),
      scrollbar_value_changed_(false) {
}

void TestScrollbar::RunTest() {
  instance_->LogTest("HandleEvent", TestHandleEvent());
}

std::string TestScrollbar::TestHandleEvent() {
  pp::Rect location;
  location.set_width(1000);
  location.set_height(1000);
  scrollbar_.SetLocation(location);

  scrollbar_.SetDocumentSize(10000);

  PP_InputEvent event;
  std::memset(&event, 0, sizeof(event));
  event.type = PP_INPUTEVENT_TYPE_KEYDOWN;
  event.u.key.modifier = 0;
  event.u.key.key_code = 0x28; // VKEY_DOWN
  scrollbar_.HandleEvent(event);

  return scrollbar_value_changed_ ?
      "" : "Didn't get callback for scrollbar value change";
}

void TestScrollbar::InvalidateWidget(pp::Widget_Dev widget,
                                     const pp::Rect& dirty_rect) {
}

void TestScrollbar::ScrollbarValueChanged(pp::Scrollbar_Dev scrollbar,
                                          uint32_t value) {
  if (scrollbar == scrollbar_)
    scrollbar_value_changed_ = true;
}
