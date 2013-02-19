// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_scrollbar.h"

#include <cstring>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Scrollbar);

TestScrollbar::TestScrollbar(TestingInstance* instance)
    : TestCase(instance),
      WidgetClient_Dev(instance),
      scrollbar_(instance, true),
      scrollbar_value_changed_(false) {
}

bool TestScrollbar::Init() {
  return CheckTestingInterface();
}

void TestScrollbar::RunTests(const std::string& filter) {
  RUN_TEST(HandleEvent, filter);
}

std::string TestScrollbar::TestHandleEvent() {
  pp::Rect location;
  location.set_width(1000);
  location.set_height(1000);
  scrollbar_.SetLocation(location);

  scrollbar_.SetDocumentSize(10000);

  pp::Core* core = pp::Module::Get()->core();
  pp::KeyboardInputEvent input_event(
      instance_, PP_INPUTEVENT_TYPE_KEYDOWN,
      core->GetTimeTicks(),
      0,  // Modifier.
      0x28,  // Key code = VKEY_DOWN.
      pp::Var());
  scrollbar_.HandleEvent(input_event);

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

void TestScrollbar::ScrollbarOverlayChanged(pp::Scrollbar_Dev scrollbar,
                                            bool type) {
}
